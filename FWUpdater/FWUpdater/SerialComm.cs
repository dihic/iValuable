using System.Collections.Generic;
using System.IO;
using System.IO.Ports;
using System.Threading;

namespace FWUpdater
{
    public enum CommandType : byte
    {
        Access = 0xc2,
        Write = 0xc4,
        WriteInfo = 0xce,
        Info = 0xcf,
        Update = 0xd0,
        Devices = 0xd1,
        Ver = 0xd2,
        Status = 0xfe
    };

    class SerialComm
    {
        public delegate void DataArrival(CommandType code, byte[] data);
        public event DataArrival DataArrivalEvent;

        private enum StateType : byte
        {
            Delimiter1,
            Delimiter2,
            FileCommand,
            FileDataLength,
            FileData,
            Checksum,
        };

        private readonly SerialPort serial;

        private const int DefaultTimeout = 100;
        private int timeout = DefaultTimeout;

        public int Timeout { set { timeout = value; } }
        public void TimeoutDefault() { timeout = DefaultTimeout; }

        public SerialComm(string port)
        {
            serial = new SerialPort(port, 115200, Parity.None, 8, StopBits.One)
            {
                Handshake = Handshake.None, //Handshake not needed   
            };
            serial.DataReceived += SerialDataReceived;
            serial.Open();
            serial.DiscardOutBuffer();
            serial.DiscardInBuffer();
        }

        private StateType dataState = StateType.Delimiter1;
        private byte sumcheck;
        private byte command;
        private ushort readIndex;
        private ushort readLength;
        private static readonly byte[] Header = { 0xfe, 0xfa, 0xfb };
        private readonly AutoResetEvent syncEvent = new AutoResetEvent(false);

        private byte[] recieveData;

        private CommandType? syncCommand;

        private readonly Queue<KeyValuePair<CommandType, byte[]>> messageQueue = 
            new Queue<KeyValuePair<CommandType, byte[]>>(); 

        // ReSharper disable once FunctionComplexityOverflow
        private void SerialDataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            var count = serial.BytesToRead;
            while (count > 0)
            {
                var b = (byte)serial.ReadByte();
                switch (dataState)
                {
                    case StateType.Delimiter1:
                        if (b == Header[0])
                            dataState = StateType.Delimiter2;
                        break;
                    case StateType.Delimiter2:
                        if (b == Header[2])
                        {
                            dataState = StateType.FileCommand;
                            sumcheck = (byte)(Header[0] + Header[2]);
                        }
                        else
                            dataState = StateType.Delimiter1;
                        break;
                    case StateType.FileCommand:
                        sumcheck += b;
                        command = b;
                        readIndex = 0;
                        readLength = 0;
                        dataState = StateType.FileDataLength;
                        break;
                    case StateType.FileDataLength:
                        sumcheck += b;
                        readLength |= (ushort)(b << (readIndex << 3));
                        if (++readIndex > 1)
                        {
                            if (readLength == 0)
                            {
                                dataState = StateType.Checksum;
                            }
                            else
                            {
                                dataState = StateType.FileData;
                                recieveData = new byte[readLength];
                                readIndex = 0;
                            }
                        }
                        break;
                    case StateType.FileData:
                        sumcheck += b;
                        recieveData[readIndex++] = b;
                        if (readIndex >= readLength)
                            dataState = StateType.Checksum;
                        break;
                    case StateType.Checksum:
                        dataState = StateType.Delimiter1;
                        if (sumcheck == b)
                        {
                            if (syncCommand == null)
                            {
                                if (DataArrivalEvent != null)
                                    DataArrivalEvent((CommandType)command, recieveData);
                            }
                            else
                            {
                                var resultData = new byte[recieveData.Length];
                                recieveData.CopyTo(resultData, 0);
                                messageQueue.Enqueue(new KeyValuePair<CommandType, byte[]>((CommandType) command,
                                    resultData));
                                syncEvent.Set();
                            }
                        }
                        break;
                }
                count--;
            }
        }

        public void SendCommand(CommandType code, byte[] data, bool async = false)
        {
            if (!async)
            {
                syncEvent.Reset();
                syncCommand = code;
            }
            Send(code, data);
            if (async)
                return;
            
            if (!syncEvent.WaitOne(timeout))
                throw new IOException("Communication No Response!");
            syncCommand = null;
            var message = messageQueue.Dequeue();
            if (DataArrivalEvent != null)
                DataArrivalEvent(message.Key, message.Value);
        }

        private void Send(CommandType com, byte[] data)
        {
            var len = 0;
            if (data != null)
                len = data.Length;
            var pre = new[] { Header[0], Header[1], (byte)com, (byte)(len & 0xff), (byte)((len >> 8) & 0xff) };
            byte checksum = 0;
            for (var i = 0; i < 5; i++)
                checksum += pre[i];
            for (var i = 0; i < len; i++)
                if (data != null)
                    checksum += data[i];
            serial.Write(pre, 0, 5);
            if (len > 0 && data != null)
                serial.Write(data, 0, len);
            pre[0] = checksum;
            serial.Write(pre, 0, 1);
        }
    }
}


