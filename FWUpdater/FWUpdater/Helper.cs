using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;

namespace FWUpdater
{
    public enum UnitType : byte
    {
        Same = 0,
        Indpt = 0x80,
        Unity = 0x81,
        Rfid = 0x82,
        Locker = 0x83
    };

    public class Helper
    {
        public static byte[] PrepareInfo(UnitType type, IReadOnlyList<byte> version, long size)
        {
            var info = new byte[8];
            info[0] = 0xAA;
            info[1] = 0xBB;
            info[2] = (byte)type;
            info[3] = 0xff;
            info[4] = version[0];
            info[5] = version[1];
            info[6] = (byte)(size & 0xff);
            info[7] = (byte)(size >> 8);
            return info;
        }

        public static bool ParseInfo(IReadOnlyList<byte> info, int offset)
        {
            if (offset + 8 > info.Count)
                return false;
            if (info[offset] != 0xAA || info[offset + 1] != 0xBB)
                return false;
            Console.WriteLine(" Type: " + (UnitType)info[offset + 2]);
            Console.WriteLine(" Version: " + (int)info[offset + 4] + "." + (int)info[offset + 5]);
            Console.WriteLine(" File Size: " + ((info[offset + 7] << 8) | info[offset + 6]) + " bytes");
            Console.WriteLine(info[offset + 3] == 0 ? " File Completed" : " File Uncompleted!");
            Console.WriteLine();
            return true;
        }

        public static void ValidateCode(byte[] data)
        {
            if (data.Length < 0x20)
                return;
            uint checksum = 0;
            for (var i = 0; i < 7; ++i)
                checksum += BitConverter.ToUInt32(data, i * 4);
            checksum = ~checksum + 1;
            var checksumBytes = BitConverter.GetBytes(checksum);
            checksumBytes.CopyTo(data, 0x1C);
        }

        public static byte[] ConvertVersion(string vstr)
        {
            var r = new Regex("(\\d+)(\\.)(\\d+)", RegexOptions.IgnoreCase | RegexOptions.Singleline);
            var m = r.Match(vstr);
            if (!m.Success)
                return null;
            var numbers = vstr.Split('.');
            var bytes = new byte[numbers.Length];
            var i = 0;
            foreach (var s in numbers)
            {
                if (byte.TryParse(s, out bytes[i]))
                    ++i;
            }
            return i != 2 ? null : bytes;
        }
    }
}
