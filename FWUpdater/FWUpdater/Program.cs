﻿using System;
using System.Collections.Generic;
using System.Configuration;
using System.Globalization;
using System.IO;
using System.IO.Ports;
using System.Linq;
using System.Reflection;
using System.Threading;

namespace FWUpdater
{
    class Program
    {
        private readonly SerialComm comm;
        private readonly Crc32 crc = new Crc32();
        private int updateTotal;
        private int updateCount;
        private readonly AutoResetEvent updatedEvent = new AutoResetEvent(false);

        enum ErrorType
        {
            // ReSharper disable UnusedMember.Local
            NoError = 0x00,
            Unknown = 0x01,
            Parameter = 0x02,
            Checksum = 0x03,
            Failure = 0x04,
            Busy = 0x05,
            Length = 0x06,
            Generic = 0xff
        };

        void CommandResponse(CommandType code, byte[] data)
        {
            switch (code)
            {
                case CommandType.Ver:
                    Console.WriteLine("MCU FW Version " + data[0] + "." + data[1]);
                    Console.WriteLine("CPUID: "+BitConverter.ToUInt32(data,2).ToString("X8"));
                    Console.Write("Mac Address: ");
                    for (var i = 6; i < 12; i++)
                    {
                        Console.Write(data[i].ToString("X2"));
                        if (i < 11)
                            Console.Write(":");
                    }
                    Console.WriteLine();
                    break;
                case CommandType.Access:
                    Console.WriteLine("Access File Slot #" + data[0]);
                    break;
                case CommandType.Write:
                    if (data[0] == 0)
                        Console.Write(".");
                    else
                    {
                        Console.WriteLine("Done");
                        if (data.Length>=5)
                            Console.WriteLine("CRC Validation: "+(BitConverter.ToUInt32(data, 1) == crc.Value ? "Pass" : "Failed"));
                    }
                    break;
                case CommandType.WriteInfo:
                    Console.WriteLine("Done");
                    break;
                case CommandType.Info:
                    if (data.Length == 8)
                    {
                        if (!Helper.ParseInfo(data, 0))
                            Console.WriteLine("File Invalid");
                    }
                    else if (data.Length == 32)
                    {
                        for (var i = 0; i < 4; ++i)
                        {
                            Console.WriteLine("Slot #"+i);
                            if (!Helper.ParseInfo(data, i*8))
                                Console.WriteLine("File Invalid");
                        }
                    }
                    break;
                case CommandType.Update:
                    switch (data.Length)
                    {
                        case 1:
                            if (data[0] == 0xff)
                                updatedEvent.Set();
                            break;
                        case 2:
                            if (data[0] == 0xff)
                                updatedEvent.Set();
                            else
                            {
                                ++updateTotal;
                                if (data[1] != 0)
                                    ++updateCount;
                                Console.WriteLine(" #Device 0x" + data[0].ToString("x2") +
                                                  ((data[1] == 0) ? " Update Failed!" : " Updated"));
                            }
                            break;
                        case 3:
                            ++updateTotal;
                            if (data[2] != 0)
                                ++updateCount;
                            Console.WriteLine(" #Device 0x" + (data[1]&0xf).ToString("x1") + data[0].ToString("x2") +
                                              ((data[2] == 0) ? " Update Failed!" : " Updated"));
                            break;

                    }
                    break;
                case CommandType.Devices:
                    var addr10Bit = (data[0]*5 + 1 == data.Length);
                    if (!addr10Bit && (data.Length == 0 || data[0] * 4 + 1 != data.Length))
                    {
                        Console.WriteLine("Response Error!");
                        break;
                    }
                    if (data[0] == 0)
                    {
                        Console.WriteLine("No Device Connected!");
                        break;
                    }
                    Console.WriteLine("There " + ((data[0] == 1) ? "is only one device":"are "+(int)data[0]+" devices") + " connected.");

                    if (addr10Bit)
                    {
                        Console.WriteLine("(10bit ID Mode)");
                        for (var i = 0; i < data[0]; ++i)
                        {
                            Console.WriteLine(" #Device 0x" + ((data[i * 5 + 1]|(data[i * 5 + 2]<<8)) & 0x1ff).ToString("x3"));
                            Console.WriteLine("  Lock Control: " + ((data[i * 5 + 2] & 0x3) != 0 ? "Enable" : "Disable"));
                            Console.WriteLine("  Unit Type: " + (UnitType)data[i * 5 + 3]);
                            Console.WriteLine("  FW Version: " + (int)data[i * 5 + 4] + "." + (int)data[i * 5 + 5]);
                        }
                    }
                    else
                        for (var i = 0; i < data[0]; ++i)
                        {
                            Console.WriteLine(" #Device 0x" + (data[i*4 + 1] & 0x7f).ToString("x2"));
                            Console.WriteLine("  Lock Control: " + ((data[i*4 + 1] & 0x80) != 0 ? "Enable" : "Disable"));
                            Console.WriteLine("  Unit Type: " + (UnitType) data[i*4 + 2]);
                            Console.WriteLine("  FW Version: " + (int) data[i*4 + 3] + "." + (int) data[i*4 + 4]);
                        }
                    break;
                case CommandType.Status:
                    var errorCommand = (CommandType) data[0];
                    var errorCode = (ErrorType) data[1];
                    //if (updating)
                    //    Console.WriteLine(errorCommand + " Error: " + errorCode);
                    //else
                        throw new Exception(errorCommand + " Error: " + errorCode);
//                    break;
                default:
                    throw new ArgumentOutOfRangeException(nameof(code));
            }
        }



        private void SendFile(byte index, UnitType type, IReadOnlyList<byte> version, string path)
        {
            if (!File.Exists(path))
            {
                Console.WriteLine("File Not Existed!");
                return;
            }
            FileStream file;
            try
            {
                file = File.Open(path, FileMode.Open, FileAccess.Read);
            }
            catch (IOException)
            {
                Console.WriteLine("Open File Failed!");
                return;
            }

            if (file.Length > 32768)
            {
                file.Close();
                Console.WriteLine("File Too Large >32KB!");
                return;
            }
            Console.WriteLine("File Size: "+file.Length+" bytes");
            comm.Timeout = 2000;
            comm.SendCommand(CommandType.Access, new[] {index});
            comm.TimeoutDefault();
            Console.Write("Writing Info .");
            comm.SendCommand(CommandType.WriteInfo, Helper.PrepareInfo(type, version, file.Length));
           
            Console.Write("Uploading File");
            crc.Init();
            var first = true;
            do
            {
                var len = (int) (file.Length - file.Position);
                if (len >= 0x100)
                    len = 0x100;
                var buffer = new byte[len];
                file.Read(buffer, 0, len);
                if (first)
                {
                    Helper.ValidateCode(buffer);
                    first = false;
                }
                foreach (var b in buffer)
                {
                    crc.UpdateByte(b);
                }
                

                //Console.WriteLine(((uint)crc.Value).ToString("x8"));
                var count = 3;
                while (count > 0)
                {
                    try
                    {
                        comm.SendCommand(CommandType.Write, buffer);
                        break;
                    }
                    catch (Exception)
                    {
                        count--;
                    }
                }
                if (count <= 0)
                {
                    Console.WriteLine("Write Fail!");
                    break;
                }

            } while (file.Position < file.Length);

            var left = 4 - (file.Length & 3);
            if (left!=4)
                for (var i = 0; i < left; ++i)
                    crc.UpdateByte(0xff);
            file.Close();
        }

        public void UpdateAll()
        {
            Console.WriteLine("Updating Started...");
            comm.SendCommand(CommandType.Update, new byte[] {0, 0, 0, 0}, true);
            updatedEvent.WaitOne(Timeout.Infinite);
            if (updateTotal==0)
                Console.WriteLine("None Updated");
            else
                Console.WriteLine(updateCount+" of "+updateTotal+" Devices Updated Successfully");
        }

        public void Update(UnitType type, ushort id = 0xffff)
        {
            Console.WriteLine("Updating Started...");
            comm.SendCommand(CommandType.Update,
                new[] {(byte) (id == 0xffff ? 1 : 2), (byte) type, (byte) (id & 0xff), (byte) (id >> 8)}, true);
            updatedEvent.WaitOne(Timeout.Infinite);
            if (updateTotal == 0)
                Console.WriteLine("None Updated");
            else
                Console.WriteLine(updateCount + " of " + updateTotal + " Devices Updated Successfully");
        }

        public Program()
        {
            var port = ConfigurationManager.AppSettings["PORT"];
            var ports = SerialPort.GetPortNames();
            if (ports.Length == 0)
            {
                Console.WriteLine("No available comm port!");
                return;
            }
            if (port == "AUTO")
                port = ports[0];
            if (ports.All(p => p != port))
            {
                Console.WriteLine("No available comm port!");
                return;
            }
            comm = new SerialComm(port);
            comm.DataArrivalEvent += CommandResponse;
        }

        public bool IsUsable => comm != null;

        public void ReadInfo(byte index = 0xff)
        {
            comm.SendCommand(CommandType.Info, index == 0xff ? null : new[] {index});
        }

        // ReSharper disable once InconsistentNaming
        public void ReadMCU()
        {
            comm.SendCommand(CommandType.Ver, null);
        }

        private void ReadDevices()
        {
            comm.SendCommand(CommandType.Devices, null);
        }

        private static void Main(string[] args)
        {
            if (args.Length == 0)
            {
                Console.WriteLine("Unit FW Online Updater");
                Console.WriteLine("Ver " + Assembly.GetExecutingAssembly().GetName().Version);
                Console.WriteLine();
                Console.WriteLine("No parameters");
                Console.WriteLine("Available commands: ");
                Console.WriteLine(" Ver");
                Console.WriteLine(" Write (index) (type) (version) (filepath)");
                Console.WriteLine(" Info [(index)]");
                Console.WriteLine(" Devices");
                Console.WriteLine(" Update All");
                Console.WriteLine(" Update (type)");
                Console.WriteLine(" Update (id) [(type)]");
                Console.WriteLine("Parameters comment: ");
                Console.WriteLine(" (index): 0 to 3 for memory slot");
                Console.WriteLine(" (type): Unity|Indpt|Rfid|Locker");
                Console.WriteLine(" (version): nn.nn");
                Console.WriteLine(" (filepath): binary executable file");
                Console.WriteLine(" (id): 0 to 0x7F for 7-bit node id specified");
                Console.WriteLine("       0 to 0x1FF for 9-bit node id specified");
                return;
            }
            var program = new Program();
            if (!program.IsUsable)
                return;
            CommandType command;
            if (!Enum.TryParse(args[0], true, out command))
            {
                Console.WriteLine("Invalid Command!");
                return;
            }

            try
            {
                UnitType type;
                byte index;
                switch (command)
                {
                    case CommandType.Ver:
                        program.ReadMCU();
                        break;
                    case CommandType.Write:
                        if (args.Length != 5)
                        {
                            Console.WriteLine("Invalid Parameters!");
                            return;
                        }
                        if (!byte.TryParse(args[1], out index))
                        {
                            Console.WriteLine("Invalid Index!");
                            return;
                        }
                        if (index >= 4)
                        {
                            Console.WriteLine("Index Out of Range!");
                            return;
                        }
                        if (!Enum.TryParse(args[2], true, out type))
                        {
                            Console.WriteLine("Invalid Type!");
                            return;
                        }
                        if (type == UnitType.Same)
                        {
                            Console.WriteLine("Invalid Type!");
                            return;
                        }
                        var ver = Helper.ConvertVersion(args[3]);
                        if (ver == null)
                        {
                            Console.WriteLine("Invalid Version Format!");
                            return;
                        }
                        program.SendFile(index, type, ver, args[4]);
                        break;
                    case CommandType.Info:
                        if (args.Length > 2)
                        {
                            Console.WriteLine("Invalid Parameters!");
                            return;
                        }
                        if (args.Length == 2)
                        {
                            if (!byte.TryParse(args[1], out index))
                            {
                                Console.WriteLine("Invalid Index!");
                                return;
                            }
                            program.ReadInfo(index);
                        }
                        else
                            program.ReadInfo();
                        break;
                    case CommandType.Devices:
                        if (args.Length > 1)
                        {
                            Console.WriteLine("Invalid Parameters!");
                            return;
                        }
                        program.ReadDevices();
                        break;
                    case CommandType.Update:
                        if (args.Length < 2)
                        {
                            Console.WriteLine("Invalid Parameters!");
                            return;
                        }
                        if (args[1].ToLower() == "all")
                            program.UpdateAll();
                        else
                        {
                            int id;
                            var isNum = args[1].StartsWith("0x")
                                ? int.TryParse(args[1].Substring(2), NumberStyles.HexNumber, new CultureInfo("en-US"),
                                    out id)
                                : int.TryParse(args[1], out id);
                            if (isNum)
                            {
                                if (args.Length > 3)
                                {
                                    Console.WriteLine("Invalid Parameters!");
                                    return;
                                }
                                //if (id < 0 || id > 127)
                                //{
                                //    Console.WriteLine("Id Out of Range!");
                                //    return;
                                //}
                                if (args.Length == 3)
                                {
                                    if (!Enum.TryParse(args[2], true, out type))
                                    {
                                        Console.WriteLine("Invalid Type!");
                                        return;
                                    }
                                    program.Update(type, (ushort) id);
                                }
                                else
                                    program.Update(UnitType.Same, (ushort) id);
                            }
                            else if (Enum.TryParse(args[1], true, out type))
                            {
                                if (type == UnitType.Same)
                                {
                                    Console.WriteLine("Invalid Type!");
                                    return;
                                }
                                if (args.Length > 2)
                                {
                                    Console.WriteLine("Invalid Parameters!");
                                    return;
                                }
                                program.Update(type);
                            }
                            else
                                Console.WriteLine("Invalid Parameters!");
                        }
                        break;
                    default:
                        Console.WriteLine("Invalid Command!");
                        return;
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.Message);
            }
        }
    }
}
