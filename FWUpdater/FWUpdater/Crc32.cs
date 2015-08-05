namespace FWUpdater
{
//Order = 32;
//Polynom = 0x4c11db7;
//Direct = 1;
//CRC Init = 0xffffffff;
//CRC Final Xor = 0x00000000;
//reverse data bytes : False
//reverse CRC result before Final XOR : False

    public class Crc32
    {
        private const uint KCrcPoly = 0x04C11DB7u;//0xEDB88320u; //
        private const uint KInitial = 0xFFFFFFFF;
        private static readonly uint[] Table;
//        private const uint CrcNumTables = 8;

        static Crc32()
        {
            //unchecked
            //{
            //    Table = new uint[256 * CrcNumTables];
            //    uint i;
            //    for (i = 0; i < 256; i++)
            //    {
            //        var r = i;
            //        for (var j = 0; j < 8; j++)
            //            r = (r >> 1) ^ (KCrcPoly & ~((r & 1) - 1));
            //        Table[i] = r;
            //    }
            //    for (; i < 256 * CrcNumTables; i++)
            //    {
            //        var r = Table[i - 256];
            //        Table[i] = Table[r & 0xFF] ^ (r >> 8);
            //    }
            //}

            Table = new uint[256];

            const uint crchighbit = 1u << 31;
            const uint crcmask = (((1u << 31) - 1) << 1) | 1;

            for (var i = 0; i < 256; i++)
            {

                var crc = (uint) i;
                crc <<= 24;

                for (var j = 0; j < 8; j++)
                {

                    var bit = crc & crchighbit;
                    crc <<= 1;
                    if (bit != 0)
                        crc ^= KCrcPoly;
                }

                crc &= crcmask;
                Table[i] = crc;
            }
        }

        public Crc32()
        {
            Init();
        }

        /// <summary>
        /// Reset CRC
        /// </summary>
        public void Init()
        {
            Value = KInitial;
        }

        public uint Value { get; private set; }

        public void UpdateByte(byte b)
        {
            Value = (Value << 8) ^ Table[(byte)(Value >> 24) ^ b];
            //value = (value >> 8) ^ Table[(byte)value ^ b];
        }

        //public void Update(byte[] data, int offset, int count)
        //{
        //    // ReSharper disable once ObjectCreationAsStatement
        //    new ArraySegment<byte>(data, offset, count);     // check arguments
        //    if (count == 0) return;

        //    var table = Table;        // important for performance!

        //    var crc = value;

        //    for (; (offset & 7) != 0 && count != 0; count--)
        //        crc = (crc >> 8) ^ table[(byte)crc ^ data[offset++]];

        //    if (count >= 8)
        //    {
        //        /*
        //         * Idea from 7-zip project sources (http://7-zip.org/sdk.html)
        //         */

        //        var to = (count - 8) & ~7;
        //        count -= to;
        //        to += offset;

        //        while (offset != to)
        //        {
        //            crc ^= (uint)(data[offset] + (data[offset + 1] << 8) + (data[offset + 2] << 16) + (data[offset + 3] << 24));
        //            var high = (uint)(data[offset + 4] + (data[offset + 5] << 8) + (data[offset + 6] << 16) + (data[offset + 7] << 24));
        //            offset += 8;

        //            crc = table[(byte)crc + 0x700]
        //                ^ table[(byte)(crc >>= 8) + 0x600]
        //                ^ table[(byte)(crc >>= 8) + 0x500]
        //                ^ table[/*(byte)*/(crc >> 8) + 0x400]
        //                ^ table[(byte)(high) + 0x300]
        //                ^ table[(byte)(high >>= 8) + 0x200]
        //                ^ table[(byte)(high >>= 8) + 0x100]
        //                ^ table[/*(byte)*/(high >> 8) + 0x000];
        //        }
        //    }

        //    while (count-- != 0)
        //        crc = (crc >> 8) ^ table[(byte)crc ^ data[offset++]];

        //    value = crc;
        //}

        //static public int Compute(byte[] data, int offset, int size)
        //{
        //    var crc = new Crc32();
        //    crc.Update(data, offset, size);
        //    return crc.Value;
        //}

        //static public int Compute(byte[] data)
        //{
        //    return Compute(data, 0, data.Length);
        //}

        //static public int Compute(ArraySegment<byte> block)
        //{
        //    return Compute(block.Array, block.Offset, block.Count);
        //}
    }
}