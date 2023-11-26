
void STREAM_register(streamCtrl_t *streamCtrl)
{
    DPRINT_V(PRNT_INFO, "Registering Stream\r\n");
    streamCtrl_t* prev = ltem_findStreamFromCntxt(streamCtrl->dataCntxt, streamType__ANY);

    if (prev != NULL)
        return;

    for (size_t i = 0; i < ltem__streamCnt; i++)
    {
        if (g_lqLTEM.streams[i] == NULL)
        {
            g_lqLTEM.streams[i] = streamCtrl;
            switch (streamCtrl->streamType)
            {
                // case streamType_file:
                //     g_lqLTEM.urcEvntHndlrs[i] = file_urcHandler;         // file module has no URC events
                //     break;
                
                // case streamType_HTTP:
                //     g_lqLTEM.urcEvntHndlrs[i] = HTTP_urcHandler;
                //     break;

                case streamType_MQTT:
                    g_lqLTEM.urcEvntHndlrs[i] = MQTT_urcHandler;
                    break;

                case streamType_SCKT:
                    g_lqLTEM.urcEvntHndlrs[i] = SCKT_urcHandler;
                    break;
            }
            return;
        }
    }
}


void STREAM_deregister(streamCtrl_t *streamCtrl)
{
    for (size_t i = 0; i < ltem__streamCnt; i++)
    {
        if (g_lqLTEM.streams[i]->dataCntxt == streamCtrl->dataCntxt)
        {
            ASSERT(memcmp(g_lqLTEM.streams[i], streamCtrl, sizeof(streamCtrl_t)) == 0);     // compare the common fields
            g_lqLTEM.streams[i] = NULL;
            return;
        }
    }
}


streamCtrl_t* STREAM_find(uint8_t dataCntxt, streamType_t streamType);
{
    for (size_t i = 0; i < ltem__streamCnt; i++)
    {
        if (g_lqLTEM.streams[i] != NULL && g_lqLTEM.streams[i]->dataCntxt == context)
        {
            if (streamType == streamType__ANY)
            {
                return g_lqLTEM.streams[i];
            }
            else if (g_lqLTEM.streams[i]->streamType == streamType)
            {
                return g_lqLTEM.streams[i];
            }
            else if (streamType == streamType_SCKT)
            {
                if (g_lqLTEM.streams[i]->streamType == streamType_UDP ||
                    g_lqLTEM.streams[i]->streamType == streamType_TCP ||
                    g_lqLTEM.streams[i]->streamType == streamType_SSLTLS)
                {
                    return g_lqLTEM.streams[i];
                }
            }
        }
    }
    return NULL;
}
