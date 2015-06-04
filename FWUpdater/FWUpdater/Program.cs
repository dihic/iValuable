using System;
using System.Collections.Generic;
using System.Configuration;
using System.IO;
using System.IO.Ports;
using System.Linq;

namespace FWUpdater
{
    class Program
    {
        private readonly SerialComm comm;

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
                    break;
                case CommandType.Status:
                    var errorCommand = (CommandType) data[0];
                    var errorCode = (ErrorType) data[1];
                    throw new Exception(errorCommand + " Error: " + errorCode);
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

            comm.SendCommand(CommandType.Access, new[] {index});
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
                try
                {
                    comm.SendCommand(CommandType.Write, buffer);
                }
                catch (Exception)
                {
                    file.Close();
                    throw;
                }
                //Thread.Sleep(500);
            } while (file.Position < file.Length);
            file.Close();
        }

        public Program()
        {
            var port = ConfigurationManager.AppSettings["PORT"];
            var ports = SerialPort.GetPortNames();
            if (ports.Length == 0)
            {
                Console.WriteLine("No available comm port!");
                //Console.ReadLine();
                return;
            }
            if (port == "AUTO")
                port = ports[0];
            if (ports.All(p => p != port))
            {
                Console.WriteLine("No available comm port!");
                //Console.ReadLine();
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

        static void Main(string[] args)
        {
            if (args.Length == 0)
            {
                Console.WriteLine("No parameters");
                Console.WriteLine("Available commands: ");
                Console.WriteLine(" Write (index) (type) (version) (filepath)");
                Console.WriteLine(" Info [(index)]");
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
                case CommandType.Update:
                    break;
                default:
                    Console.WriteLine("Invalid Command!");
                    return;
            }
        }
    }
}
