using System;
using System.Collections.Generic;
using System.Configuration;
using System.IO.Ports;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace FWUpdater
{
    class Program
    {
        public readonly SerialComm Comm;

        public Program()
        {
            var port = ConfigurationManager.AppSettings["PORT"];
            var ports = SerialPort.GetPortNames();
            if (ports.Length == 0)
            {
                Console.WriteLine("No available comm port!");
                Console.ReadLine();
                return;
            }
            if (port == "AUTO")
                port = ports[0];
            if (ports.All(p => p != port))
            {
                Console.WriteLine("No available comm port!");
                Console.ReadLine();
                return;
            }
            Comm = new SerialComm(port);
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
                Console.WriteLine(" (type): Unity/Indpt/Rfid");
                Console.WriteLine(" (version): nn.nn");
                Console.WriteLine(" (filepath): binary executable file");
                Console.WriteLine(" (id): 0 to 127 for node id specified");
                return;
            }
            byte[] data;
            var program = new Program();
            CommandType command;
            if (!Enum.TryParse(args[0], out command))
            {
                Console.WriteLine("Invalid Command!");
                return;
            }
            switch (command)
            {
                case CommandType.Write:
                    break;
                case CommandType.Info:
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
