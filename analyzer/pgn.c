#include <common.h>
#include "pgn.h"

# define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

#ifndef min
# define min(x,y) ((x)<=(y)?(x):(y))
#endif
#ifndef max
# define max(x,y) ((x)>=(y)?(x):(y))
#endif

Pgn* searchForPgn(int pgn) {
  int first = 0;
  int end = ARRAY_SIZE(pgnList);
  int count = end - first;
  while (count > 0) {
    int step = count / 2;
    int mid = first + step;
    if (pgnList[mid].pgn < pgn) {
      first = mid + 1;
      count -= step + 1;
    } else {
      count = step;
    }
  }
  if (first < ARRAY_SIZE(pgnList)) {
    return pgnList + first;

  }
  return 0;
}

Pgn* endPgn(Pgn* first) {
  Pgn *p = first;
  while (p != pgnList + ARRAY_SIZE(pgnList) && p->pgn == first->pgn) {
    ++p;
  }
  return p;
}

Pgn* getMatchingPgn(int pgnId, uint8_t *dataStart, int length) {
  Pgn * firstPgn = searchForPgn(pgnId);
  Pgn* pgn;
  Pgn* end = endPgn(firstPgn);
  int i;

  if (!firstPgn) {
    return 0;
  }

  for (pgn = firstPgn; pgn != end; ++pgn) {
    int startBit = 0;
    uint8_t* data = dataStart;

    bool matchedFixedField = true;
    bool hasFixedField = false;
    const Field* field;

    /* There is a next index that we can use as well. We do so if the 'fixed' fields don't match */

    // Iterate over fields
    for (i = 0, startBit = 0, data = dataStart, field = pgn->fieldList + i;
         field->name && field->size && i < pgn->fieldCount; i++)
    {
      int bits = field->size;

      if (field->units && field->units[0] == '=')
      {
        int64_t value, desiredValue;
        int64_t maxValue;

        hasFixedField = true;
        extractNumber(field, data, startBit, field->size, &value, &maxValue);
        desiredValue = strtol(field->units + 1, 0, 10);
        if (value != desiredValue)
        {
          matchedFixedField = false;
          break;
        }
      }
      startBit += bits;
      data += startBit / 8;
      startBit %= 8;
    }
    if (! hasFixedField || (hasFixedField && matchedFixedField))
    {
      return pgn;
    }
  }
  return 0;
}

Field * getField(uint32_t pgnId, uint32_t field)
{
  int index;

  Pgn* pgn = searchForPgn(pgnId);
  if (pgn && field < pgn->fieldCount) {
    return pgn->fieldList + field;
  }
  return 0;
}

/*
 *
 * This is perhaps as good a place as any to explain how CAN messages are layed out by the
 * NMEA. Basically, it's a mess once the bytes are recomposed into bytes (the on-the-wire
 * format is fine).
 *
 * For fields that are aligned on bytes there isn't much of an issue, they appear in our
 * buffers in standard Intel 'least endian' format.
 * For instance the MMSI # 244050447 is, in hex: 0x0E8BEA0F. This will be found in the CAN data as:
 * byte x+0: 0x0F
 * byte x+1: 0xEA
 * byte x+2: 0x8B
 * byte x+3: 0x0e
 *
 * To gather together we loop over the bytes, and keep increasing the magnitude of what we are
 * adding:
 *    for (i = 0, magnitude = 0; i < 4; i++)
 *    {
 *      value += data[i] << magnitude;
 *      magnitude += 8;
 *    }
 *
 * However, when there are two bit fields after each other, lets say A of 2 and then B of 6 bits:
 * then that is layed out MSB first, so the bit mask is 0b11000000 for the first
 * field and 0b00111111 for the second field.
 *
 * This means that if we have a bit field that crosses a byte boundary and does not start on
 * a byte boundary, the bit masks are like this (for a 16 bit field starting at the 3rd bit):
 *
 * 0b00111111 0b11111111 0b11000000
 *     ------   --------   --
 *     000000   11110000   11
 *     543210   32109876   54
 *
 * So we are forced to mask bits 0 and 1 of the first byte. Since we need to process the previous
 * field first, we cannot repeatedly shift bits out of the byte: if we shift left we get the first
 * field first, but in MSB order. We need bit values in LSB order, as the next byte will be more
 * significant. But we can't shift right as that will give us bits in LSB order but then we get the
 * two fields in the wrong order...
 *
 * So for that reason we explicitly test, per byte, how many bits we need and how many we have already
 * used.
 *
 */

void extractNumber(const Field * field, uint8_t * data, size_t startBit, size_t bits, int64_t * value, int64_t * maxValue)
{
  bool hasSign = field->hasSign;

  size_t firstBit = startBit;
  size_t bitsRemaining = bits;
  size_t magnitude = 0;
  size_t bitsInThisByte;
  uint64_t bitMask;
  uint64_t allOnes;
  uint64_t valueInThisByte;

  *value = 0;
  *maxValue = 0;

  while (bitsRemaining)
  {
    bitsInThisByte = min(8 - firstBit, bitsRemaining);
    allOnes = (uint64_t) ((((uint64_t) 1) << bitsInThisByte) - 1);

    //How are bits ordered in bytes for bit fields? There are two ways, first field at LSB or first
    //field as MSB.
    //Experimentation, using the 129026 PGN, has shown that the most likely candidate is LSB.
    bitMask = allOnes << firstBit;
    valueInThisByte = (*data & bitMask) >> firstBit;

    *value |= valueInThisByte << magnitude;
    *maxValue |= (int64_t) allOnes << magnitude;

    magnitude += bitsInThisByte;
    bitsRemaining -= bitsInThisByte;
    firstBit += bitsInThisByte;
    if (firstBit >= 8)
    {
      firstBit -= 8;
      data++;
    }
  }

  if (hasSign)
  {
    *maxValue >>= 1;

    if (field->offset) /* J1939 Excess-K notation */
    {
      *value += field->offset;
    }
    else
    {
      bool negative = (*value & (((uint64_t) 1) << (bits - 1))) > 0;

      if (negative)
      {
        /* Sign extend value for cases where bits < 64 */
        /* Assume we have bits = 16 and value = -2 then we do: */
        /* 0000.0000.0000.0000.0111.1111.1111.1101 value    */
        /* 0000.0000.0000.0000.0111.1111.1111.1111 maxvalue */
        /* 1111.1111.1111.1111.1000.0000.0000.0000 ~maxvalue */
        *value |= ~*maxValue;
      }
    }
  }
}
