namespace LTEm_MQTT_Host
{
    using HiveMQtt.Client;
    using HiveMQtt.Client.Options;
    using HiveMQtt.MQTT5.ReasonCodes;
    using HiveMQtt.MQTT5.Types;
    using System.Text.Json;
    using System.Threading.Tasks;

    internal class Program
    {
        static readonly string C2D_Topic = "lq_c2d";
        static readonly string D2C_Topic = "lq_d2c";


        static async Task Main(string[] args)
        {
            Console.WriteLine("Hello, World!");


            /* These options will need to be updated for your HiveMQ Cloud accounts
             */
            var mqttOptions = new HiveMQClientOptions
            {
                Host = "e9cb510d802c43dba6a0d79bd6a20a4c.s1.eu.hivemq.cloud",
                Port = 8883,
                UseTLS = true,
                UserName = "host_consoleApp_1",
                Password = "9p6M!SPi!ETp9FGo"
            };

            var mqttClient = new HiveMQClient(mqttOptions);


            Console.WriteLine($"Connecting to {mqttOptions.Host} on port {mqttOptions.Port} ...");

            // Connect
            HiveMQtt.Client.Results.ConnectResult connectResult;
            try
            {
                connectResult = await mqttClient.ConnectAsync().ConfigureAwait(false);
                if (connectResult.ReasonCode == ConnAckReasonCode.Success)
                {
                    Console.WriteLine($"Connect successful: {connectResult}");
                }
                else
                {
                    // FIXME: Add ToString
                    Console.WriteLine($"Connect failed: {connectResult}");
                    Environment.Exit(-1);
                }
            }
            catch (System.Net.Sockets.SocketException e)
            {
                Console.WriteLine($"Error connecting to the MQTT Broker with the following socket error: {e.Message}");
                Environment.Exit(-1);
            }
            catch (Exception e)
            {
                Console.WriteLine($"Error connecting to the MQTT Broker with the following message: {e.Message}");
                Environment.Exit(-1);
            }


            mqttClient.OnMessageReceived += (sender, args) =>
            {
                Console.WriteLine($"Message Received: {args.PublishMessage.PayloadAsString}");
            };

            await mqttClient.SubscribeAsync(D2C_Topic).ConfigureAwait(false);

            Console.WriteLine();

            int indx = 0;
            while (true) 
            {
                indx++;

                string msg = $"{{ \"info\": {indx} }}";
                var pubResult = await mqttClient.PublishAsync(C2D_Topic, msg, QualityOfService.AtLeastOnceDelivery).ConfigureAwait(false);
                Console.WriteLine($"Sent: {msg}");

                await Task.Delay(10000);
            }
        }
    }
}
