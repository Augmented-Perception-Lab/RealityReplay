using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Net.Sockets;
using System.Text;
using UnityEngine;
using UnityEngine.Networking;
using static UnityEngine.Networking.NetworkTransport;

namespace Assets.Script.Remote
{
    public delegate void MessageAvailableEventHandler(object sender,
        MessageAvailableEventArgs e);

    public class MessageAvailableEventArgs : EventArgs
    {
        public MessageAvailableEventArgs(string message) : base()
        {
            this.Message = message;
        }

        public string Message { get; private set; }
    }

    /// <summary>
    /// Simple TCP Client that connects to the Python inference server.
    /// </summary>
    public class Client : MonoBehaviour
    {
        private TcpClient socket;
        private NetworkStream stream;
        private String bindIP = "localhost";
        //private String bindIP = "192.168.0.110";
        //private int bindPort = 9998;
        private Byte[] sizeBuffer = new Byte[4];

        private bool _connectedToServer = false;

        public event MessageAvailableEventHandler MessageAvailable;

        public void ConnectToTcpServer(int port)
        {
            try
            {
                socket = new TcpClient();
                socket.NoDelay = true;
                socket.Connect(bindIP, port);
                stream = socket.GetStream();
                _connectedToServer = true;
                Debug.Log("connected");
            }
            catch (Exception e)
            {
                Debug.Log("On client connect exception " + e);
            }
        }

        public void Close()
        {
            if (!_connectedToServer)
            {
                return;
            }

            Debug.Log("closing connection to server");
            stream.Close();
            socket.Close();
        }

        protected void OnMessageAvailable(MessageAvailableEventArgs e)
        {
            var handler = MessageAvailable;
            if (handler != null)
                handler(this, e);
            string[] message = e.Message.Split(',');
            string signal = message[0];
            string param;
            if (message.Length > 1)
                param = message[1];
            switch (signal)
            {
                case "MARK_PRIMARY_REGION":
                    /*
                     Save current view information (e.g., angle, coordinate) as primary region.
                     */
                    break;
                case "LEFT_PRIMARY_REGION":
                    /*
                     Place the frame for visualization at the saved point of primary region, referencing to SAVE_DIR
                     */
                    // SAVE_DIR = param; 
                    break;
                case "PLAY_VIZ":
                    /*Show images in SAVE_DIR on the frame*/
                    break;
                default:
                    Debug.Log("Didn't get anything usable from the server!");
                    break;

            }
        }

        public void ListenAsync()
        {
            if (!_connectedToServer)
            {
                return;
            }

            // asynchronous read, calls the callback once data is available
            stream.BeginRead(sizeBuffer, 0, sizeBuffer.Length, new AsyncCallback(ReadCallback), null);
            Debug.Log("begin read finished");
        }

        private void ReadCallback(IAsyncResult ar)
        {
            // this blocks until the data is ready
            int bytesRead = stream.EndRead(ar);

            Debug.Log("end read finished " + bytesRead);

            if (bytesRead != 2)
                throw new InvalidOperationException("Invalid message header.");


            int messageSize = BitConverter.ToInt32(sizeBuffer, 0);
            //        int messageSize = BitConverter.ToInt32(sizeBuffer, 0);
            Debug.Log("message size " + messageSize);

            // now read the actual message
            ReadMessage(messageSize);

            // start listening again
            ListenAsync();
        }

        private void ReadMessage(int messageSize)
        {
            try
            {

                int remainingLength = messageSize;
                string serverMessage = "";

                Debug.Log("getting message of size " + messageSize);

                while (remainingLength > 0)
                {
                    var incomingData = new Byte[remainingLength];
                    int bytes_read = stream.Read(incomingData, 0, remainingLength);

                    if (bytes_read > 0)
                    {
                        serverMessage += Encoding.ASCII.GetString(incomingData);
                    }
                    else
                    {
                        Debug.Log("something went wrong when reading data");
                    }

                    remainingLength -= bytes_read;
                }

                Debug.Log("read message " + serverMessage);

                // do something with the servermessage here
                OnMessageAvailable(new MessageAvailableEventArgs(serverMessage));
            }

            catch (SocketException socketException)
            {
                Debug.Log("Socket exception when receiving: " + socketException);
            }

        }

        public string ListenSync()
        {
            try
            {

                if (!_connectedToServer)
                {
                    return "";
                }

                Byte[] bytes = new Byte[4];
                string serverMessage = "";

                // Read first 4 bytes for length of the message
                int length = stream.Read(bytes, 0, bytes.Length);
                if (length == 4)
                {
                    int msgLength = BitConverter.ToInt32(bytes, 0);
                    var allData = new List<Byte>();

                    while (msgLength > 0)
                    {
                        var incomingData = new Byte[msgLength];
                        int bytes_read = stream.Read(incomingData, 0, msgLength);
                        // Debug.Log("Msg length read " + bytes_read);

                        if (bytes_read > 0)
                        {
                            //serverMessage += Encoding.UTF8.GetString(incomingData);
                            allData.AddRange(incomingData.Take(bytes_read));
                        }
                        else
                        {
                            Debug.Log("something went wrong when reading data");
                        }

                        msgLength -= bytes_read;
                    }

                    serverMessage = Encoding.UTF8.GetString(allData.ToArray());

                }

                return serverMessage;
            }
            catch (SocketException socketException)
            {
                Debug.Log("Socket exception when receiving: " + socketException);
            }
            return "";
        }


        public byte[] ListenSyncRaw()
        {
            try
            {

                if (!_connectedToServer || !stream.CanRead)
                {
                    return new byte[0];
                }

                Byte[] bytes = new Byte[4];
                var serverMessage = new List<Byte>();

                try
                {

                    // Read first 4 bytes for length of the message
                    int length = stream.Read(bytes, 0, bytes.Length);
                    if (length == 4)
                    {
                        int msgLength = BitConverter.ToInt32(bytes, 0);

                        while (msgLength > 0)
                        {
                            var incomingData = new Byte[msgLength];
                            int bytes_read = stream.Read(incomingData, 0, msgLength);
                            // Debug.Log("Msg length read " + bytes_read);

                            if (bytes_read > 0)
                            {
                                //serverMessage += Encoding.UTF8.GetString(incomingData);
                                serverMessage.AddRange(incomingData.Take(bytes_read));
                            }
                            else
                            {
                                Debug.Log("something went wrong when reading data");
                            }

                            msgLength -= bytes_read;
                        }

                    }

                    return serverMessage.ToArray();
                }
                catch (Exception e)
                {
                    Debug.LogWarning("Exception when reading stream");
                    Debug.LogWarning(e);
                    return new byte[0];
                }
            }
            catch (SocketException socketException)
            {
                Debug.Log("Socket exception when receiving: " + socketException);
            }

            return new byte[0];
        }

        public bool Send(string msg)
        {
            if (socket == null || stream == null)
            {
                Debug.LogWarning("Could not send sync. Socket or stream null.");
                return false;
            }
            try

            {
                if (stream.CanWrite)
                {
                    // Convert string message to byte array
                    byte[] clientMessageAsByteArray = Encoding.ASCII.GetBytes(msg);
                    byte[] length = BitConverter.GetBytes((int)clientMessageAsByteArray.Length);

                    var all = new byte[length.Length + clientMessageAsByteArray.Length];
                    length.CopyTo(all, 0);
                    clientMessageAsByteArray.CopyTo(all, length.Length);

                    // Write byte array to socketConnection stream.                 
                    stream.Write(all, 0, all.Length);
                    return true;
                }

                return false;
            }
            catch (SocketException socketException)
            {
                Debug.Log("Socket exception when sending: " + socketException);
                return false;
            }
        }

        public bool Send(byte[] buffer)
        {
            if (socket == null || stream == null)
            {
                Debug.LogWarning("Could not send sync. Socket or stream null.");
                return false;
            }

            try
            {
                if (stream.CanWrite)
                {
                    // Convert string message to byte array
                    byte[] bufferSize = BitConverter.GetBytes(buffer.Length);

                    var all = new byte[bufferSize.Length + buffer.Length];
                    bufferSize.CopyTo(all, 0);
                    buffer.CopyTo(all, bufferSize.Length);

                    // Write byte array to socketConnection stream.                 
                    stream.BeginWrite(all, 0, all.Length, null, null);

                    return true;
                }
                else
                {
                    return false;
                }
            }
            catch (SocketException socketException)
            {
                Debug.Log("Socket exception when sending: " + socketException);
                return false;
            }
        }

    }
}