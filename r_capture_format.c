/*
Copyright 2011 Jared Krinke.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

unsigned int r_capture_signature = 0x04151e0;

unsigned int r_capture_rle_compress(int *dest, const int *source, unsigned int source_entries)
{
    int previous_value = source[0];
    unsigned int occurences = 1;
    unsigned int i;
    unsigned int j = 0;

    for (i = 1; i < source_entries; ++i)
    {
        const int value = source[i];

        if (value != previous_value)
        {
            /* End of a run */
            if (occurences == 1)
            {
                /* Unique value; encode directly */
                dest[j++] = previous_value;
            }
            else
            {
                /* Two or more repeated values; encode using "xxn" where "x" is the value and "n" is the number of
                 * additional repetitions */
                dest[j++] = previous_value;
                dest[j++] = previous_value;
                dest[j++] = occurences - 2;
            }

            occurences = 1;
        }
        else
        {
            ++occurences;
        }

        previous_value = value;
    }

    /* Final run */
    if (occurences == 1)
    {
        dest[j++] = previous_value;
    }
    else
    {
        dest[j++] = previous_value;
        dest[j++] = previous_value;
        dest[j++] = occurences - 2;
    }

    return j;
}

unsigned int r_capture_rle_decompress(int *dest, const int *source, unsigned int source_entries)
{
    int last_value = source[0];
    unsigned int i;
    unsigned int j = 0;

    dest[j++] = source[0];

    for (i = 1; i < source_entries; ++i)
    {
        const int value = source[i];
        int repetitions = 1;
        int k;

        if (value == last_value)
        {
            repetitions += source[++i];
        }

        for (k = 0; k < repetitions; ++k)
        {
            dest[j++] = value;
        }

        last_value = value;
    }

    return j;
}
