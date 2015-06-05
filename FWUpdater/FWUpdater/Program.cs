using System;
using System.Collections.Generic;
using System.Configuration;
using System.IO;
using System.IO.Ports;
using System.Linq;
using System.Threading;

namespace FWUpdater
{
    class Program
    {
        private readonly SerialComm comm;
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
                case CommandType.Access:
                    Console.WriteLine("Access File Slot #" + data[0]);
                    break;
                case CommandType.Write:
                    if (data[0] == 0)
                        Console.Write(".");
                    else
                        Console.WriteLine("Done");
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
                    if (data[0] == 0xff)
                        updatedEvent.Set();
                    else
                    {
                        ++updateTotal;
                        if (data[1] != 0)
                            ++updateCount;
                        Console.WriteLine(" #Device 0x" + data[0].ToString("x2") +
                                          ((data[1] == 0) ? " Update Failed!" : "Updated"));
                    }
                    break;
                case CommandType.Devices:
                    if (data.Length == 0 || data[0]*4 + 1 != data.Length)
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
                    for (var i = 0; i < data[0]; ++i)
                    {
                        Console.WriteLine(" #Device 0x" + (data[i*4 + 1]&0x7f).ToString("x2"));
                        Console.WriteLine("  Lock Control: "+ ((data[i*4+1]&0x80)!=0 ? "Enable":"Disable"));
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
                    throw new ArgumentOutOfRangeException("code");
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
                        if (count-- != 0) continue;
                        file.Close();
                        throw;
                    }
                }
            } while (file.Position < file.Length);
            file.Close();
        }

        public void UpdateAll()
        {
            Console.WriteLine("Updating Started...");
            comm.SendCommand(CommandType.Update, new byte[] {0, 0, 0}, true);
            updatedEvent.WaitOne(Timeout.Infinite);
            if (updateTotal==0)
                Console.WriteLine("None Updated");
            else
                Console.WriteLine(updateCount+" of "+updateTotal+" Devices Updated Successfully");
        }

        public void Update(UnitType type, byte id = 0xff)
        {
            Console.WriteLine("Updating Started...");
            comm.SendCommand(CommandType.Update, new[] {(byte) (id == 0xff ? 1 : 2), (byte) type, id}, true);
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

        public bool IsUsable { get { return comm != null; } }

        public void ReadInfo(byte index = 0xff)
        {
            comm.SendCommand(CommandType.Info, index == 0xff ? null : new[] {index});
        }

        private void ReadDevices()
        {
            comm.SendCommand(CommandType.Devices, null);
        }

        private static void Main(string[] args)
        {
            if (args.Length == 0)
            {
                Console.WriteLine("No parameters");
                Console.WriteLine("Available commands: ");
                Console.WriteLine(" Write (index) (type) (version) (filepath)");
                Console.WriteLine(" Info [(index)]");
                Console.WriteLine(" Devices");
                Console.WriteLine(" Update All");
                Console.WriteLine(" Update (type)");
                Console.WriteLine(" Update (id) [(type)]");
                Console.WriteLine("Parameters comment: ");
                Console.WriteLine(" (index): 0 to 3 for memory slot");
                Console.WriteLine(" (type): Unity|Indpt|Rfid");
                Console.WriteLine(" (version): nn.nn");
                Console.WriteLine(" (filepath): binary executable file");
                Console.WriteLine(" (id): 0 to 127 for node id specified");
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
            byte index;
            UnitType type;

            try
            {
                switch (command)
                {
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
                            if (Enum.TryParse(args[1], true, out type))
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
                            {
                                int id;
                                if (args.Length > 3 || !int.TryParse(args[1], out id))
                                {
                                    Console.WriteLine("Invalid Parameters!");
                                    return;
                                }
                                if (id < 0 || id > 127)
                                {
                                    Console.WriteLine("Id Out of Range!");
                                    return;
                                }
                                if (args.Length == 3)
                                {
                                    if (!Enum.TryParse(args[2], true, out type))
                                    {
                                        Console.WriteLine("Invalid Type!");
                                        return;
                                    }
                                    program.Update(type, (byte) id);
                                }
                                else
                                    program.Update(UnitType.Same, (byte) id);
                            }
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
