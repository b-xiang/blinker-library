#if defined(ESP8266) || defined(ESP32)

#define BLINKER_PRO

#include <EEPROM.h>

#include "modules/WebSockets/WebSocketsServer.h"
#include "modules/mqtt/Adafruit_MQTT.h"
#include "modules/mqtt/Adafruit_MQTT_Client.h"
#include "modules/ArduinoJson/ArduinoJson.h"

#include "Adapters/BlinkerPRO.h"
#include "Blinker/BlinkerConfig.h"
#include "Blinker/BlinkerDebug.h"
#include "utility/BlinkerUtility.h"

char*       MQTT_HOST_PRO;
char*       MQTT_ID_PRO;
char*       MQTT_NAME_PRO;
char*       MQTT_KEY_PRO;
char*       MQTT_PRODUCTINFO_PRO;
char*       UUID_PRO;
char*       AUTHKEY_PRO;
char*       MQTT_DEVICEID_PRO;
// char*       DEVICE_NAME;
char*       BLINKER_PUB_TOPIC_PRO;
char*       BLINKER_SUB_TOPIC_PRO;
uint16_t MQTT_PORT_PRO;


#if defined(ESP8266)
    BearSSL::WiFiClientSecure   client_pro;
    // WiFiClientSecure            client_pro;
#elif defined(ESP32)
    WiFiClientSecure            client_s_PRO;
#endif

WiFiClient              client_PRO;
Adafruit_MQTT_Client*       mqtt_PRO;
// Adafruit_MQTT_Publish   *iotPub;
Adafruit_MQTT_Subscribe*    iotSub_PRO;

#define WS_SERVERPORT       81
WebSocketsServer webSocket_PRO = WebSocketsServer(WS_SERVERPORT);

char*   msgBuf_PRO;
bool    isFresh_PRO = false;
bool    isConnect_PRO = false;
bool    isAvail_PRO = false;
uint8_t ws_num_PRO = 0;
uint8_t dataFrom_PRO = BLINKER_MSG_FROM_MQTT;

void webSocketEvent_PRO(uint8_t num, WStype_t type, \
                    uint8_t * payload, size_t length)
{

    switch(type)
    {
        case WStype_DISCONNECTED:
            BLINKER_LOG_ALL(BLINKER_F("Disconnected! "), num);

            isConnect_PRO = false;
            break;
        case WStype_CONNECTED:
            {
                IPAddress ip = webSocket_PRO.remoteIP(num);
                
                BLINKER_LOG_ALL(BLINKER_F("num: "), num, \
                                BLINKER_F(", Connected from: "), ip, 
                                BLINKER_F(", url: "), (char *)payload);
                
                // send message to client_PRO
                webSocket_PRO.sendTXT(num, "{\"state\":\"connected\"}\n");

                ws_num_PRO = num;

                isConnect_PRO = true;
            }
            break;
        case WStype_TEXT:
            BLINKER_LOG_ALL(BLINKER_F("num: "), num, \
                            BLINKER_F(", get Text: "), (char *)payload, \
                            BLINKER_F(", length: "), length);
            
            if (length < BLINKER_MAX_READ_SIZE) {
                if (isFresh_PRO) free(msgBuf_PRO);
                msgBuf_PRO = (char*)malloc((length+1)*sizeof(char));
                strcpy(msgBuf_PRO, (char*)payload);
                isAvail_PRO = true;
                isFresh_PRO = true;
            }

            dataFrom_PRO = BLINKER_MSG_FROM_WS;

            ws_num_PRO = num;

            // send message to client_PRO
            // webSocket_PRO.sendTXT(num, "message here");

            // send data to all connected clients
            // webSocket_PRO.broadcastTXT("message here");
            break;
        case WStype_BIN:
            // BLINKER_LOG("num: ", num, " get binary length: ", length);
            // hexdump(payload, length);

            // send message to client_PRO
            // webSocket_PRO.sendBIN(num, payload, length);
            break;
    }
}

BlinkerPRO::BlinkerPRO() { isHandle = &isConnect_PRO; }


bool BlinkerPRO::connect()
{
    int8_t ret;

    webSocket_PRO.loop();

    if (mqtt_PRO->connected())
    {
        return true;
    }

    disconnect();

    if ((millis() - latestTime) < 5000)
    {
        return false;
    }

    BLINKER_LOG(BLINKER_F("Connecting to MQTT... "));

    #if defined(ESP8266)
        client_pro.setInsecure();
    #endif

    if ((ret = mqtt_PRO->connect()) != 0)
    {
        BLINKER_LOG(mqtt_PRO->connectErrorString(ret));
        BLINKER_LOG(BLINKER_F("Retrying MQTT connection in 5 seconds..."));

        this->latestTime = millis();
        return false;
    }
    
    BLINKER_LOG(BLINKER_F("MQTT Connected!"));
    BLINKER_LOG_FreeHeap();

    this->latestTime = millis();

    return true;
}

bool BlinkerPRO::connected()
{ 
    if (!isMQTTinit)
    {
        return *isHandle;
    }

    return mqtt_PRO->connected() || *isHandle; 
}

bool BlinkerPRO::mConnected()
{
    if (!isMQTTinit) return false;
    else return mqtt_PRO->connected();
}

void BlinkerPRO::disconnect()
{
    mqtt_PRO->disconnect();

    if (*isHandle) webSocket_PRO.disconnect();
}

void BlinkerPRO::ping()
{
    BLINKER_LOG_ALL(BLINKER_F("MQTT Ping!"));

    if (!mqtt_PRO->ping())
    {
        disconnect();
        delay(100);

        connect();
    }
    else
    {
        this->latestTime = millis();
    }
}

bool BlinkerPRO::available()
{
    webSocket_PRO.loop();

    checkKA();

    if (!mqtt_PRO->connected() || \
        (millis() - this->latestTime) > BLINKER_MQTT_PING_TIMEOUT)
    {
        ping();
    }
    else
    {
        subscribe();
    }

    if (isAvail_PRO)
    {
        isAvail_PRO = false;
        return true;
    }
    else {
        return false;
    }
}

bool BlinkerPRO::aligenieAvail()
{
    if (isAliAvail)
    {
        isAliAvail = false;
        return true;
    }
    else {
        return false;
    }
}

bool BlinkerPRO::extraAvailable()
{
    if (isBavail)
    {
        isBavail = false;        
        return true;
    }
    else
    {
        return false;
    }
}

void BlinkerPRO::subscribe()
{
    if (!isMQTTinit) return;

    Adafruit_MQTT_Subscribe *subscription;
    while ((subscription = mqtt_PRO->readSubscription(10)))
    {
        if (subscription == iotSub_PRO)
        {
            BLINKER_LOG_ALL(BLINKER_F("Got: "), (char *)iotSub_PRO->lastread);
            
            DynamicJsonBuffer jsonBuffer;
            JsonObject& root = jsonBuffer.parseObject(String((char *)iotSub_PRO->lastread));

            String _uuid = root["fromDevice"];
            String dataGet = root["data"];
            
            BLINKER_LOG_ALL(BLINKER_F("data: "), dataGet);
            BLINKER_LOG_ALL(BLINKER_F("fromDevice: "), _uuid);
            
            if (strcmp(_uuid.c_str(), UUID_PRO) == 0)
            {
                BLINKER_LOG_ALL(BLINKER_F("Authority uuid"));
                
                kaTime = millis();
                isAvail_PRO = true;
                isAlive = true;
            }
            else if (_uuid == BLINKER_CMD_ALIGENIE)
            {
                BLINKER_LOG_ALL(BLINKER_F("form AliGenie"));
                
                aliKaTime = millis();
                isAliAlive = true;
                isAliAvail = true;
            }
            else
            {
                // dataGet = String((char *)iotSub_PRO->lastread);
                root.printTo(dataGet);
                
                BLINKER_ERR_LOG_ALL(BLINKER_F("No authority uuid, \
                                    check is from bridge/share device, \
                                    data: "), dataGet);
                
                // return;

                isBavail = true;
            }

            // memset(msgBuf_PRO, 0, BLINKER_MAX_READ_SIZE);
            // memcpy(msgBuf_PRO, dataGet.c_str(), dataGet.length());

            if (isFresh_PRO) free(msgBuf_PRO);
            msgBuf_PRO = (char*)malloc((dataGet.length()+1)*sizeof(char));
            strcpy(msgBuf_PRO, dataGet.c_str());
            isFresh_PRO = true;
            
            this->latestTime = millis();

            dataFrom_PRO = BLINKER_MSG_FROM_MQTT;
        }
    }
}

char * BlinkerPRO::lastRead()
{
    if (isFresh_PRO) return msgBuf_PRO;
    return "";
}

void BlinkerPRO::flush()
{
    if (isFresh_PRO)
    {
        free(msgBuf_PRO); isFresh_PRO = false; isAvail_PRO = false;
        isAliAvail = false; isBavail = false;
    }
}

bool BlinkerPRO::print(char * data, bool needCheck)
{
    // BLINKER_LOG_FreeHeap();
    if (*isHandle && dataFrom_PRO == BLINKER_MSG_FROM_WS)
    {
        if (needCheck)
        {
            if (!checkPrintSpan())
            {
                respTime = millis();
                return false;
            }
        }

        respTime = millis();
        
        BLINKER_LOG_ALL(BLINKER_F("WS response: "));
        BLINKER_LOG_ALL(data);
        BLINKER_LOG_ALL(BLINKER_F("Succese..."));

        strcat(data, BLINKER_CMD_NEWLINE);
        
        webSocket_PRO.sendTXT(ws_num_PRO, data);

        return true;
    }
    else
    {
        // // String payload;
        // if (STRING_contains_string(data, BLINKER_CMD_NEWLINE))
        // {
        //     uint8_t num = strlen(data) - 1;
        //     for(uint8_t c_num = num; c_num > 0; c_num--)
        //     {
        //         data[c_num+7] = data[c_num-1];
        //     }
        //     // payload = BLINKER_F("{\"data\":");
        //     // payload += data.substring(0, data.length() - 1);
        //     // payload += BLINKER_F(",\"fromDevice\":\"");
        //     // payload += MQTT_ID_PRO;
        //     // payload += BLINKER_F("\",\"toDevice\":\"");
        //     // payload += UUID_PRO;
        //     // payload += BLINKER_F("\",\"deviceType\":\"OwnApp\"}");
        // }
        // else
        // {
        //     uint8_t num = strlen(data);
        //     for(uint8_t c_num = num; c_num > 0; c_num--)
        //     {
        //         data[c_num+7] = data[c_num-1];
        //     }
        //     // payload = BLINKER_F("{\"data\":");
        //     // payload += data;
        //     // payload += BLINKER_F(",\"fromDevice\":\"");
        //     // payload += MQTT_ID_PRO;
        //     // payload += BLINKER_F("\",\"toDevice\":\"");
        //     // payload += UUID_PRO;
        //     // payload += BLINKER_F("\",\"deviceType\":\"OwnApp\"}");
        // }

        uint8_t num = strlen(data);
        for(uint8_t c_num = num; c_num > 0; c_num--)
        {
            data[c_num+7] = data[c_num-1];
        }

        String data_add = BLINKER_F("{\"data\":");
        for(uint8_t c_num = 0; c_num < 8; c_num++)
        {
            data[c_num] = data_add[c_num];
        }

        data_add = BLINKER_F(",\"fromDevice\":\"");
        strcat(data, data_add.c_str());
        strcat(data, MQTT_ID_PRO);
        data_add = BLINKER_F("\",\"toDevice\":\"");
        strcat(data, data_add.c_str());
        strcat(data, UUID_PRO);
        data_add = BLINKER_F("\",\"deviceType\":\"OwnApp\"}");
        strcat(data, data_add.c_str());

        if (!isJson(STRING_format(data))) return false;
        
        BLINKER_LOG_ALL(BLINKER_F("MQTT Publish..."));
        BLINKER_LOG_FreeHeap_ALL();
        
        bool _alive = isAlive;
        

        if (needCheck)
        {
            if (!checkPrintSpan())
            {
                return false;
            }
            respTime = millis();
        }

        if (mqtt_PRO->connected())
        {
            if (needCheck)
            {
                if (!checkCanPrint())
                {
                    if (!_alive)
                    {
                        isAlive = false;
                    }
                    return false;
                }
            }

            if (! mqtt_PRO->publish(BLINKER_PUB_TOPIC_PRO, data))
            {
                BLINKER_LOG_ALL(data);
                BLINKER_LOG_ALL(BLINKER_F("...Failed"));
                BLINKER_LOG_FreeHeap_ALL();
                
                if (!_alive)
                {
                    isAlive = false;
                }
                return false;
            }
            else
            {
                BLINKER_LOG_ALL(data);
                BLINKER_LOG_ALL(BLINKER_F("...OK!"));
                BLINKER_LOG_FreeHeap_ALL();
                
                if (needCheck) printTime = millis();

                if (!_alive)
                {
                    isAlive = false;
                }
                return true;
            }            
        }
        else
        {
            BLINKER_ERR_LOG(BLINKER_F("MQTT Disconnected"));
            isAlive = false;
            return false;
        }
    }
}

bool BlinkerPRO::bPrint(char * name, const String & data)
{
    // String payload;
    // if (STRING_contains_string(data, BLINKER_CMD_NEWLINE))
    // {
    //     payload = BLINKER_F("{\"data\":");
    //     payload += data.substring(0, data.length() - 1);
    //     payload += BLINKER_F(",\"fromDevice\":\"");
    //     payload += MQTT_ID_PRO;
    //     payload += BLINKER_F("\",\"toDevice\":\"");
    //     payload += name;
    //     payload += BLINKER_F("\",\"deviceType\":\"DiyBridge\"}");
    // }
    // else
    // {
    //     payload = BLINKER_F("{\"data\":");
    //     payload += data;
    //     payload += BLINKER_F(",\"fromDevice\":\"");
    //     payload += MQTT_ID_PRO;
    //     payload += BLINKER_F("\",\"toDevice\":\"");
    //     payload += name;
    //     payload += BLINKER_F("\",\"deviceType\":\"DiyBridge\"}");
    // }

    // uint8_t num = strlen(data);
    // for(uint8_t c_num = num; c_num > 0; c_num--)
    // {
    //     data[c_num+7] = data[c_num-1];
    // }

    String data_add = BLINKER_F("{\"data\":");
    // for(uint8_t c_num = 0; c_num < 8; c_num++)
    // {
    //     data[c_num] = data_add[c_num];
    // }

    data_add += data;
    data_add += BLINKER_F(",\"fromDevice\":\"");
    data_add += MQTT_ID_PRO;
    data_add += BLINKER_F("\",\"toDevice\":\"");
    data_add += name;
    data_add += BLINKER_F("\",\"deviceType\":\"DiyBridge\"}");

    // data_add = BLINKER_F(",\"fromDevice\":\"");
    // strcat(data, data_add.c_str());
    // strcat(data, MQTT_ID_PRO);
    // data_add = BLINKER_F("\",\"toDevice\":\"");
    // strcat(data, data_add.c_str());
    // strcat(data, name);
    // data_add = BLINKER_F("\",\"deviceType\":\"DiyBridge\"}");
    // strcat(data, data_add.c_str());

    if (!isJson(data_add)) return false;

    BLINKER_LOG_ALL(BLINKER_F("MQTT Bridge Publish..."));

    // bool _alive = isAlive;
    // bool state = STRING_contains_string(data, BLINKER_CMD_NOTICE);

    // if (!state) {
    //     state = (STRING_contains_string(data, BLINKER_CMD_STATE) 
    //         && STRING_contains_string(data, BLINKER_CMD_ONLINE));
    // }

    if (mqtt_PRO->connected()) {
        // if (!state) {
        if (!checkCanBprint()) {
            // if (!_alive) {
            //     isAlive = false;
            // }
            return false;
        }
        // }

        // Adafruit_MQTT_Publish iotPub = Adafruit_MQTT_Publish(mqtt_PRO, BLINKER_PUB_TOPIC_PRO);

        // if (! iotPub.publish(payload.c_str())) {

        String bPubTopic = BLINKER_F("");

        // if (mqtt_broker == BLINKER_MQTT_BORKER_ONENET)
        // {
        //     bPubTopic = MQTT_PRODUCTINFO_PRO;
        //     bPubTopic += BLINKER_F("/");
        //     bPubTopic += name;
        //     bPubTopic += BLINKER_F("/r");
        // }
        // else
        // {
        //     bPubTopic = BLINKER_PUB_TOPIC_PRO;
        // }

        if (! mqtt_PRO->publish(BLINKER_PUB_TOPIC_PRO, data_add.c_str()))
        {
            BLINKER_LOG_ALL(data_add);
            BLINKER_LOG_ALL(BLINKER_F("...Failed"));
            
            // if (!_alive) {
            //     isAlive = false;
            // }
            return false;
        }
        else
        {
            BLINKER_LOG_ALL(data_add);
            BLINKER_LOG_ALL(BLINKER_F("...OK!"));
            
            bPrintTime = millis();

            // if (!_alive) {
            //     isAlive = false;
            // }
            return true;
        }            
    }
    else
    {
        BLINKER_ERR_LOG(BLINKER_F("MQTT Disconnected"));
        // isAlive = false;
        return false;
    }
    // }
}

bool BlinkerPRO::aliPrint(const String & data)
{
    // String payload;

    // payload = BLINKER_F("{\"data\":");
    // payload += data;
    // payload += BLINKER_F(",\"fromDevice\":\"");
    // payload += MQTT_ID_PRO;
    // payload += BLINKER_F("\",\"toDevice\":\"AliGenie_r\"");
    // payload += BLINKER_F(",\"deviceType\":\"vAssistant\"}");

    // uint8_t num = strlen(data);
    // for(uint8_t c_num = num; c_num > 0; c_num--)
    // {
    //     data[c_num+7] = data[c_num-1];
    // }

    String data_add = BLINKER_F("{\"data\":");
    // for(uint8_t c_num = 0; c_num < 8; c_num++)
    // {
    //     data[c_num] = data_add[c_num];
    // }

    // data_add = BLINKER_F(",\"fromDevice\":\"");
    // strcat(data, data_add.c_str());
    // strcat(data, MQTT_ID_PRO);
    // data_add = BLINKER_F("\",\"toDevice\":\"AliGenie_r\"");
    // strcat(data, data_add.c_str());
    // data_add = BLINKER_F(",\"deviceType\":\"vAssistant\"}");
    // strcat(data, data_add.c_str());

    data_add += data;
    data_add += BLINKER_F(",\"fromDevice\":\"");
    data_add += MQTT_ID_PRO;
    data_add += BLINKER_F("\",\"toDevice\":\"AliGenie_r\"");
    data_add += BLINKER_F(",\"deviceType\":\"vAssistant\"}");

    if (!isJson(data_add)) return false;
            
    BLINKER_LOG_ALL(BLINKER_F("MQTT AliGenie Publish..."));
    BLINKER_LOG_FreeHeap_ALL();

    if (mqtt_PRO->connected())
    {
        if (!checkAliKA())
        {
            return false;
        }

        if (!checkAliPrintSpan())
        {
            respAliTime = millis();
            return false;
        }
        respAliTime = millis();

        // Adafruit_MQTT_Publish iotPub = Adafruit_MQTT_Publish(mqtt_PRO, BLINKER_PUB_TOPIC_PRO);

        // if (! iotPub.publish(payload.c_str())) {

        if (! mqtt_PRO->publish(BLINKER_PUB_TOPIC_PRO, data_add.c_str()))
        {
            BLINKER_LOG_ALL(data_add);
            BLINKER_LOG_ALL(BLINKER_F("...Failed"));
            BLINKER_LOG_FreeHeap_ALL();
            
            isAliAlive = false;
            return false;
        }
        else
        {
            BLINKER_LOG_ALL(data_add);
            BLINKER_LOG_ALL(BLINKER_F("...OK!"));
            BLINKER_LOG_FreeHeap_ALL();
            
            isAliAlive = false;
            return true;
        }      
    }
    else
    {
        BLINKER_ERR_LOG(BLINKER_F("MQTT Disconnected"));
        return false;
    }
}

// void BlinkerPRO::aliType(const String & type)
// {
//     _aliType = (char*)malloc((type.length()+1)*sizeof(char));
//     strcpy(_aliType, type.c_str());
//     BLINKER_LOG_ALL(BLINKER_F("_aliType: "), _aliType);
// }

void BlinkerPRO::begin(const char* _type)
{
    _deviceType = _type;
    
    BLINKER_LOG_ALL(BLINKER_F("PRO deviceType: "), _type);

    mDNSInit();
}

bool BlinkerPRO::autoPrint(uint32_t id)
{
    String payload = BLINKER_F("{\"data\":{\"set\":{");
    payload += BLINKER_F("\"trigged\":true,\"autoData\":{");
    payload += BLINKER_F("\"autoId\":");
    payload += STRING_format(id);
    payload += BLINKER_F("}}}");
    payload += BLINKER_F(",\"fromDevice\":\"");
    payload += STRING_format(MQTT_ID_PRO);
    payload += BLINKER_F("\",\"toDevice\":\"autoManager\"}");
        // "\",\"deviceType\":\"" + "type" + "\"}";

    BLINKER_LOG_ALL(BLINKER_F("autoPrint..."));

    if (mqtt_PRO->connected())
    {
        if ((millis() - linkTime) > BLINKER_LINK_MSG_LIMIT || \
            linkTime == 0)
        {
            // linkTime = millis();

            // Adafruit_MQTT_Publish iotPub = Adafruit_MQTT_Publish(mqtt_PRO, BLINKER_PUB_TOPIC_PRO);

            // if (! iotPub.publish(payload.c_str())) {

            if (! mqtt_PRO->publish(BLINKER_PUB_TOPIC_PRO, payload.c_str()))
            {
                BLINKER_LOG_ALL(payload);
                BLINKER_LOG_ALL(BLINKER_F("...Failed"));
                
                return false;
            }
            else
            {
                BLINKER_LOG_ALL(payload);
                BLINKER_LOG_ALL(BLINKER_F("...OK!"));
                
                linkTime = millis();
                return true;
            }
        }
        else
        {
            BLINKER_ERR_LOG_ALL(BLINKER_F("MQTT NOT ALIVE OR MSG LIMIT "), linkTime);
            
            return false;
        }
    }
    else
    {
        BLINKER_ERR_LOG(BLINKER_F("MQTT Disconnected"));
        return false;
    }
}

// bool BlinkerPRO::autoPrint(char *name, char *type, char *data)
// {
//     String payload = BLINKER_F("{\"data\":{");
//     payload += STRING_format(data);
//     payload += BLINKER_F("},\"fromDevice\":\"");
//     payload += STRING_format(MQTT_ID_PRO);
//     payload += BLINKER_F("\",\"toDevice\":\"");
//     payload += name;
//     payload += BLINKER_F("\",\"deviceType\":\"");
//     payload += type;
//     payload += BLINKER_F("\"}");
        
//     BLINKER_LOG_ALL(BLINKER_F("autoPrint..."));
    
//     if (mqtt_PRO->connected()) {
//         if ((millis() - linkTime) > BLINKER_LINK_MSG_LIMIT || \
//             linkTime == 0)
//         {
//             linkTime = millis();
            
//             BLINKER_LOG_ALL(payload, BLINKER_F("...OK!"));
            
//             return true;
//         }
//         else {
//             BLINKER_ERR_LOG_ALL(BLINKER_F("MQTT NOT ALIVE OR MSG LIMIT "), linkTime);
            
//             return false;
//         }
//     }
//     else
//     {
//         BLINKER_ERR_LOG(BLINKER_F("MQTT Disconnected"));
//         return false;
//     }
// }

// bool BlinkerPRO::autoPrint(char *name1, char *type1, char *data1
//     , char *name2, char *type2, char *data2)
// {
//     String payload = BLINKER_F("{\"data\":{");
//     payload += STRING_format(data1);
//     payload += BLINKER_F("},\"fromDevice\":\"");
//     payload += STRING_format(MQTT_ID_PRO);
//     payload += BLINKER_F("\",\"toDevice\":\"");
//     payload += name1;
//     payload += BLINKER_F("\",\"deviceType\":\"");
//     payload += type1;
//     payload += BLINKER_F("\"}");
        
//     BLINKER_LOG_ALL(BLINKER_F("autoPrint..."));
    
//     if (mqtt_PRO->connected())
//     {
//         if ((millis() - linkTime) > BLINKER_LINK_MSG_LIMIT || \
//             linkTime == 0)
//         {
//             linkTime = millis();

//             BLINKER_LOG_ALL(payload, BLINKER_F("...OK!"));

//             payload = BLINKER_F("{\"data\":{");
//             payload += STRING_format(data2);
//             payload += BLINKER_F("},\"fromDevice\":\"");
//             payload += STRING_format(MQTT_ID_PRO);
//             payload += BLINKER_F("\",\"toDevice\":\"");
//             payload += name2;
//             payload += BLINKER_F("\",\"deviceType\":\"");
//             payload += type2;
//             payload += BLINKER_F("\"}");
                
//             BLINKER_LOG_ALL(payload, BLINKER_F("...OK!"));
            
//             return true;
//         }
//         else
//         {
//             BLINKER_ERR_LOG_ALL(BLINKER_F("MQTT NOT ALIVE OR MSG LIMIT "), linkTime);
            
//             return false;
//         }
//     }
//     else
//     {
//         BLINKER_ERR_LOG(BLINKER_F("MQTT Disconnected"));
//         return false;
//     }
// }

char * BlinkerPRO::deviceName() { return MQTT_DEVICEID_PRO;/*MQTT_ID_PRO;*/ }

bool BlinkerPRO::authCheck()
{
    uint8_t _authCheck;
    
    BLINKER_LOG_ALL(BLINKER_F("authCheck start"));
    
    EEPROM.begin(BLINKER_EEP_SIZE);
    EEPROM.get(BLINKER_EEP_ADDR_AUTH_CHECK, _authCheck);
    if (_authCheck == BLINKER_AUTH_CHECK_DATA)
    {
        EEPROM.commit();
        EEPROM.end();
        isAuth = true;
        
        BLINKER_LOG_ALL(BLINKER_F("authCheck end"));
        
        return true;
    }
    EEPROM.commit();
    EEPROM.end();
    
    BLINKER_LOG_ALL(BLINKER_F("authCheck end"));
    
    return false;
}

bool BlinkerPRO::connectServer() {
    const int httpsPort = 443;
#if defined(ESP8266)
    String host = BLINKER_F("iotdev.clz.me");
    String fingerprint = BLINKER_F("84 5f a4 8a 70 5e 79 7e f5 b3 b4 20 45 c8 35 55 72 f6 85 5a");

    // WiFiClientSecure client_s_PRO;

    BearSSL::WiFiClientSecure client_s_PRO;
    
    BLINKER_LOG_ALL(BLINKER_F("connecting to "), host);

    // BLINKER_LOG_FreeHeap();
    
    uint8_t connet_times = 0;
    // client_s_PRO.stop();
    ::delay(100);

    bool mfln = client_s_PRO.probeMaxFragmentLength(host, httpsPort, 1024);
    if (mfln) {
        client_s_PRO.setBufferSizes(1024, 1024);
    }
    client_s_PRO.setFingerprint(fingerprint.c_str());

    while (1) {
        bool cl_connected = false;
        if (!client_s_PRO.connect(host, httpsPort)) {
            BLINKER_ERR_LOG(BLINKER_F("server connection failed"));
            connet_times++;

            ::delay(1000);
        }
        else {
            BLINKER_LOG_ALL(BLINKER_F("connection succeed"));
            cl_connected = true;

            break;
        }

        if (connet_times >= 4 && !cl_connected)  return BLINKER_CMD_FALSE;
    }

    String client_msg;

    String url_iot = BLINKER_F("/api/v1/user/device/register?deviceType=");
    url_iot += _deviceType;
    url_iot += BLINKER_F("&deviceName=");
    url_iot += macDeviceName();

    if (_deviceType == BLINKER_SMART_LAMP) {
        url_iot += BLINKER_F("&aliType=light");
    }
    else if (_deviceType == BLINKER_SMART_PLUGIN) {
        url_iot += BLINKER_F("&aliType=outlet");
    }
    else if (_deviceType == BLINKER_AIR_DETECTOR) {
        url_iot += BLINKER_F("&aliType=sensor");
    }

// #if defined(BLINKER_ALIGENIE_LIGHT)
//     url_iot += BLINKER_F("&aliType=light");
// #elif defined(BLINKER_ALIGENIE_OUTLET)
//     url_iot += BLINKER_F("&aliType=outlet");
// #elif defined(BLINKER_ALIGENIE_SWITCH)
// #elif defined(BLINKER_ALIGENIE_SENSOR)
//     url_iot += BLINKER_F("&aliType=sensor");
// #endif

    BLINKER_LOG_ALL(BLINKER_F("HTTPS begin: "), host, url_iot);
    
    client_msg = BLINKER_F("GET ");
    client_msg += url_iot;
    client_msg += BLINKER_F(" HTTP/1.1\r\nHost: ");
    client_msg += host;
    client_msg += BLINKER_F(":");
    client_msg += STRING_format(httpsPort);
    client_msg += BLINKER_F("\r\nConnection: close\r\n\r\n");

    client_s_PRO.print(client_msg);
    
    BLINKER_LOG_ALL(BLINKER_F("client_msg: "), client_msg);

    unsigned long timeout = millis();
    while (client_s_PRO.available() == 0) {
        if (millis() - timeout > 5000) {
            BLINKER_LOG_ALL(BLINKER_F(">>> Client Timeout !"));
            client_s_PRO.stop();
            return BLINKER_CMD_FALSE;
        }
    }

    String _dataGet;
    String lastGet;
    String lengthOfJson;
    while (client_s_PRO.available()) {
        // String line = client_s_PRO.readStringUntil('\r');
        _dataGet = client_s_PRO.readStringUntil('\n');

        if (_dataGet.startsWith("Content-Length: ")){
            int addr_start = _dataGet.indexOf(' ');
            int addr_end = _dataGet.indexOf('\0', addr_start + 1);
            lengthOfJson = _dataGet.substring(addr_start + 1, addr_end);
        }

        if (_dataGet == "\r") {
            BLINKER_LOG_ALL(BLINKER_F("headers received"));
            
            break;
        }
    }

    for(int i=0;i<lengthOfJson.toInt();i++){
        lastGet += (char)client_s_PRO.read();
    }

    // BLINKER_LOG_FreeHeap();

    client_s_PRO.stop();
    client_s_PRO.flush();

    // BLINKER_LOG_FreeHeap();

    _dataGet = lastGet;
    
    BLINKER_LOG_ALL(BLINKER_F("_dataGet: "), _dataGet);

    String payload = _dataGet;

#elif defined(ESP32)
    String host = BLINKER_F("https://iotdev.clz.me");
    // const char* ca = \ 
    //     "-----BEGIN CERTIFICATE-----\n" \
    //     "MIIEgDCCA2igAwIBAgIQDKTfhr9lmWbWUT0hjX36oDANBgkqhkiG9w0BAQsFADBy\n" \
    //     "MQswCQYDVQQGEwJDTjElMCMGA1UEChMcVHJ1c3RBc2lhIFRlY2hub2xvZ2llcywg\n" \
    //     "SW5jLjEdMBsGA1UECxMURG9tYWluIFZhbGlkYXRlZCBTU0wxHTAbBgNVBAMTFFRy\n" \
    //     "dXN0QXNpYSBUTFMgUlNBIENBMB4XDTE4MDEwNDAwMDAwMFoXDTE5MDEwNDEyMDAw\n" \
    //     "MFowGDEWMBQGA1UEAxMNaW90ZGV2LmNsei5tZTCCASIwDQYJKoZIhvcNAQEBBQAD\n" \
    //     "ggEPADCCAQoCggEBALbOFn7cJ2I/FKMJqIaEr38n4kCuJCCeNf1bWdWvOizmU2A8\n" \
    //     "QeTAr5e6Q3GKeJRdPnc8xXhqkTm4LOhgdZB8KzuVZARtu23D4vj4sVzxgC/zwJlZ\n" \
    //     "MRMxN+cqI37kXE8gGKW46l2H9vcukylJX+cx/tjWDfS2YuyXdFuS/RjhCxLgXzbS\n" \
    //     "cve1W0oBZnBPRSMV0kgxTWj7hEGZNWKIzK95BSCiMN59b+XEu3NWGRb/VzSAiJEy\n" \
    //     "Hy9DcDPBC9TEg+p5itHtdMhy2gq1OwsPgl9HUT0xmDATSNEV2RB3vwviNfu9/Eif\n" \
    //     "ObhsV078zf30TqdiESqISEB68gJ0Otru67ePoTkCAwEAAaOCAWowggFmMB8GA1Ud\n" \
    //     "IwQYMBaAFH/TmfOgRw4xAFZWIo63zJ7dygGKMB0GA1UdDgQWBBR/KLqnke61779P\n" \
    //     "xc9htonQwLOxPDAYBgNVHREEETAPgg1pb3RkZXYuY2x6Lm1lMA4GA1UdDwEB/wQE\n" \
    //     "AwIFoDAdBgNVHSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIwTAYDVR0gBEUwQzA3\n" \
    //     "BglghkgBhv1sAQIwKjAoBggrBgEFBQcCARYcaHR0cHM6Ly93d3cuZGlnaWNlcnQu\n" \
    //     "Y29tL0NQUzAIBgZngQwBAgEwgYEGCCsGAQUFBwEBBHUwczAlBggrBgEFBQcwAYYZ\n" \
    //     "aHR0cDovL29jc3AyLmRpZ2ljZXJ0LmNvbTBKBggrBgEFBQcwAoY+aHR0cDovL2Nh\n" \
    //     "Y2VydHMuZGlnaXRhbGNlcnR2YWxpZGF0aW9uLmNvbS9UcnVzdEFzaWFUTFNSU0FD\n" \
    //     "QS5jcnQwCQYDVR0TBAIwADANBgkqhkiG9w0BAQsFAAOCAQEAhtM4eyrWB14ajJpQ\n" \
    //     "ibZ5FbzVuvv2Le0FOSoss7UFCDJUYiz2LiV8yOhL4KTY+oVVkqHaYtcFS1CYZNzj\n" \
    //     "6xWcqYZJ+pgsto3WBEgNEEe0uLSiTW6M10hm0LFW9Det3k8fqwSlljqMha3gkpZ6\n" \
    //     "8WB0f2clXOuC+f1SxAOymnGUsSqbU0eFSgevcOIBKR7Hr3YXBXH3jjED76Q52OMS\n" \
    //     "ucfOM9/HB3jN8o/ioQbkI7xyd/DUQtzK6hSArEoYRl3p5H2P4fr9XqmpoZV3i3gQ\n" \
    //     "oOdVycVtpLunyUoVAB2DcOElfDxxXCvDH3XsgoIU216VY03MCaUZf7kZ2GiNL+UX\n" \
    //     "9UBd0Q==\n" \
    //     "-----END CERTIFICATE-----\n";
// #endif

    HTTPClient http;

    String url_iot = host;
    url_iot += BLINKER_F("/api/v1/user/device/register?deviceType=");
    url_iot += _deviceType;
    url_iot += BLINKER_F("&deviceName=");
    url_iot += macDeviceName();

    if (_deviceType == BLINKER_SMART_LAMP) {
        url_iot += BLINKER_F("&aliType=light");
    }
    else if (_deviceType == BLINKER_SMART_PLUGIN) {
        url_iot += BLINKER_F("&aliType=outlet");
    }
    else if (_deviceType == BLINKER_AIR_DETECTOR) {
        url_iot += BLINKER_F("&aliType=sensor");
    }

// #if defined(BLINKER_ALIGENIE_LIGHT)
//     url_iot += BLINKER_F("&aliType=light");
// #elif defined(BLINKER_ALIGENIE_OUTLET)
//     url_iot += BLINKER_F("&aliType=outlet");
// #elif defined(BLINKER_ALIGENIE_SWITCH)
// #elif defined(BLINKER_ALIGENIE_SENSOR)
//     url_iot += BLINKER_F("&aliType=sensor");
// #endif

    BLINKER_LOG_ALL(BLINKER_F("HTTPS begin: "), url_iot);

// #if defined(ESP8266)
//     http.begin(url_iot, fingerprint); //HTTP
// #elif defined(ESP32)
    // http.begin(url_iot, ca); TODO
    http.begin(url_iot);
// #endif
    int httpCode = http.GET();

    String payload;

    if (httpCode > 0) {
      // HTTP header has been send and Server response header has been handled

        BLINKER_LOG_ALL(BLINKER_F("[HTTP] GET... code: "), httpCode);

        // file found at server
        if (httpCode == HTTP_CODE_OK) {
            payload = http.getString();
            // BLINKER_LOG(payload);
        }
    }
    else {
        BLINKER_LOG_ALL(BLINKER_F("[HTTP] GET... failed, error: "), http.errorToString(httpCode).c_str());
        payload = http.getString();
        BLINKER_LOG_ALL(payload);
    }

    http.end();
#endif

    BLINKER_LOG_ALL(BLINKER_F("reply was:"));
    BLINKER_LOG_ALL(BLINKER_F("=============================="));
    BLINKER_LOG_ALL(payload);
    BLINKER_LOG_ALL(BLINKER_F("=============================="));

    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(payload);

    if (STRING_contains_string(payload, BLINKER_CMD_NOTFOUND) || !root.success() ||
        !STRING_contains_string(payload, BLINKER_CMD_IOTID)) {
        // while(1) {
            BLINKER_ERR_LOG(("Please make sure you have register this device!"));
            // ::delay(60000);

            return false;
        // }
    }

    // String _userID = STRING_find_string(payload, "deviceName", "\"", 4);
    // String _userName = STRING_find_string(payload, "iotId", "\"", 4);
    // String _key = STRING_find_string(payload, "iotToken", "\"", 4);
    // String _productInfo = STRING_find_string(payload, "productKey", "\"", 4);
    // String _broker = STRING_find_string(payload, "broker", "\"", 4);
    // String _uuid = STRING_find_string(payload, "uuid", "\"", 4);
    String _userID = root[BLINKER_CMD_DETAIL][BLINKER_CMD_DEVICENAME];
    String _userName = root[BLINKER_CMD_DETAIL][BLINKER_CMD_IOTID];
    String _key = root[BLINKER_CMD_DETAIL][BLINKER_CMD_IOTTOKEN];
    String _productInfo = root[BLINKER_CMD_DETAIL][BLINKER_CMD_PRODUCTKEY];
    String _broker = root[BLINKER_CMD_DETAIL][BLINKER_CMD_BROKER];
    String _uuid = root[BLINKER_CMD_DETAIL][BLINKER_CMD_UUID];
    String _authKey = root[BLINKER_CMD_DETAIL][BLINKER_CMD_KEY];

    if (_broker == BLINKER_MQTT_BORKER_ALIYUN) {
        // memcpy(DEVICE_NAME, _userID.c_str(), 12);
        String _deviceName = _userID.substring(12, 36);
        MQTT_DEVICEID_PRO = (char*)malloc((_deviceName.length()+1)*sizeof(char));
        strcpy(MQTT_DEVICEID_PRO, _deviceName.c_str());
        MQTT_ID_PRO = (char*)malloc((_userID.length()+1)*sizeof(char));
        strcpy(MQTT_ID_PRO, _userID.c_str());
        MQTT_NAME_PRO = (char*)malloc((_userName.length()+1)*sizeof(char));
        strcpy(MQTT_NAME_PRO, _userName.c_str());
        MQTT_KEY_PRO = (char*)malloc((_key.length()+1)*sizeof(char));
        strcpy(MQTT_KEY_PRO, _key.c_str());
        MQTT_PRODUCTINFO_PRO = (char*)malloc((_productInfo.length()+1)*sizeof(char));
        strcpy(MQTT_PRODUCTINFO_PRO, _productInfo.c_str());
        MQTT_HOST_PRO = (char*)malloc((strlen(BLINKER_MQTT_ALIYUN_HOST)+1)*sizeof(char));
        strcpy(MQTT_HOST_PRO, BLINKER_MQTT_ALIYUN_HOST);
        AUTHKEY_PRO = (char*)malloc((_authKey.length()+1)*sizeof(char));
        strcpy(AUTHKEY_PRO, _authKey.c_str());
        MQTT_PORT_PRO = BLINKER_MQTT_ALIYUN_PORT;
    }
    else if (_broker == BLINKER_MQTT_BORKER_QCLOUD) {
        // String id2name = _userID.subString(10, _userID.length());
        // memcpy(DEVICE_NAME, _userID.c_str(), 12);
        MQTT_DEVICEID_PRO = (char*)malloc((_userID.length()+1)*sizeof(char));
        strcpy(MQTT_DEVICEID_PRO, _userID.c_str());
        String IDtest = _productInfo + _userID;
        MQTT_ID_PRO = (char*)malloc((IDtest.length()+1)*sizeof(char));
        strcpy(MQTT_ID_PRO, IDtest.c_str());
        String NAMEtest = IDtest + ";" + _userName;
        MQTT_NAME_PRO = (char*)malloc((NAMEtest.length()+1)*sizeof(char));
        strcpy(MQTT_NAME_PRO, NAMEtest.c_str());
        MQTT_KEY_PRO = (char*)malloc((_key.length()+1)*sizeof(char));
        strcpy(MQTT_KEY_PRO, _key.c_str());
        MQTT_PRODUCTINFO_PRO = (char*)malloc((_productInfo.length()+1)*sizeof(char));
        strcpy(MQTT_PRODUCTINFO_PRO, _productInfo.c_str());
        MQTT_HOST_PRO = (char*)malloc((strlen(BLINKER_MQTT_QCLOUD_HOST)+1)*sizeof(char));
        strcpy(MQTT_HOST_PRO, BLINKER_MQTT_QCLOUD_HOST);
        MQTT_PORT_PRO = BLINKER_MQTT_QCLOUD_PORT;
    }
    else if (_broker == BLINKER_MQTT_BORKER_ONENET) {
        // memcpy(DEVICE_NAME, _userID.c_str(), 12);
        MQTT_DEVICEID_PRO = (char*)malloc((_userID.length()+1)*sizeof(char));
        strcpy(MQTT_DEVICEID_PRO, _userID.c_str());
        MQTT_ID_PRO = (char*)malloc((_userName.length()+1)*sizeof(char));
        strcpy(MQTT_ID_PRO, _userName.c_str());
        MQTT_NAME_PRO = (char*)malloc((_productInfo.length()+1)*sizeof(char));
        strcpy(MQTT_NAME_PRO, _productInfo.c_str());
        MQTT_KEY_PRO = (char*)malloc((_key.length()+1)*sizeof(char));
        strcpy(MQTT_KEY_PRO, _key.c_str());
        MQTT_PRODUCTINFO_PRO = (char*)malloc((_productInfo.length()+1)*sizeof(char));
        strcpy(MQTT_PRODUCTINFO_PRO, _productInfo.c_str());
        MQTT_HOST_PRO = (char*)malloc((strlen(BLINKER_MQTT_ONENET_HOST)+1)*sizeof(char));
        strcpy(MQTT_HOST_PRO, BLINKER_MQTT_ONENET_HOST);
        MQTT_PORT_PRO = BLINKER_MQTT_ONENET_PORT;
    }
    UUID_PRO = (char*)malloc((_uuid.length()+1)*sizeof(char));
    strcpy(UUID_PRO, _uuid.c_str());

    char _authCheck;
    EEPROM.begin(BLINKER_EEP_SIZE);
    EEPROM.get(BLINKER_EEP_ADDR_AUUID, UUID_PRO);
    if (STRING_format(UUID_PRO) != _uuid) {
        strcpy(UUID_PRO, _uuid.c_str());
        EEPROM.put(BLINKER_EEP_ADDR_AUUID, UUID_PRO);
        isNew = true;
    }
    EEPROM.get(BLINKER_EEP_ADDR_AUTH_CHECK, _authCheck);
    if (_authCheck != BLINKER_AUTH_CHECK_DATA) {
        EEPROM.put(BLINKER_EEP_ADDR_AUTH_CHECK, BLINKER_AUTH_CHECK_DATA);
        isAuth = true;
    }
    EEPROM.commit();
    EEPROM.end();
    
    BLINKER_LOG_ALL(BLINKER_F("===================="));
    BLINKER_LOG_ALL(BLINKER_F("DEVICE_NAME: "), macDeviceName());
    BLINKER_LOG_ALL(BLINKER_F("MQTT_PRODUCTINFO_PRO: "), MQTT_PRODUCTINFO_PRO);
    BLINKER_LOG_ALL(BLINKER_F("MQTT_ID_PRO: "), MQTT_ID_PRO);
    BLINKER_LOG_ALL(BLINKER_F("MQTT_NAME_PRO: "), MQTT_NAME_PRO);
    BLINKER_LOG_ALL(BLINKER_F("MQTT_KEY_PRO: "), MQTT_KEY_PRO);
    BLINKER_LOG_ALL(BLINKER_F("MQTT_BROKER: "), _broker);
    BLINKER_LOG_ALL(BLINKER_F("HOST: "), MQTT_HOST_PRO);
    BLINKER_LOG_ALL(BLINKER_F("PORT: "), MQTT_PORT_PRO);
    BLINKER_LOG_ALL(BLINKER_F("UUID_PRO: "), UUID_PRO);
    BLINKER_LOG_ALL(BLINKER_F("AUTHKEY_PRO: "), AUTHKEY_PRO);
    BLINKER_LOG_ALL(BLINKER_F("===================="));

    if (_broker == BLINKER_MQTT_BORKER_ALIYUN) {
        String PUB_TOPIC_STR = BLINKER_F("/");
        PUB_TOPIC_STR += MQTT_PRODUCTINFO_PRO;
        PUB_TOPIC_STR += BLINKER_F("/");
        PUB_TOPIC_STR += MQTT_ID_PRO;
        PUB_TOPIC_STR += BLINKER_F("/s");

        BLINKER_PUB_TOPIC_PRO = (char*)malloc((PUB_TOPIC_STR.length() + 1)*sizeof(char));
        // memcpy(BLINKER_PUB_TOPIC_PRO, PUB_TOPIC_STR.c_str(), str_len);
        strcpy(BLINKER_PUB_TOPIC_PRO, PUB_TOPIC_STR.c_str());
        
        BLINKER_LOG_ALL(BLINKER_F("BLINKER_PUB_TOPIC_PRO: "), BLINKER_PUB_TOPIC_PRO);
        
        String SUB_TOPIC_STR = BLINKER_F("/");
        SUB_TOPIC_STR += MQTT_PRODUCTINFO_PRO;
        SUB_TOPIC_STR += BLINKER_F("/");
        SUB_TOPIC_STR += MQTT_ID_PRO;
        SUB_TOPIC_STR += BLINKER_F("/r");
        
        BLINKER_SUB_TOPIC_PRO = (char*)malloc((SUB_TOPIC_STR.length() + 1)*sizeof(char));
        // memcpy(BLINKER_SUB_TOPIC_PRO, SUB_TOPIC_STR.c_str(), str_len);
        strcpy(BLINKER_SUB_TOPIC_PRO, SUB_TOPIC_STR.c_str());
        
        BLINKER_LOG_ALL(BLINKER_F("BLINKER_SUB_TOPIC_PRO: "), BLINKER_SUB_TOPIC_PRO);
    }
    else if (_broker == BLINKER_MQTT_BORKER_QCLOUD) {
        String PUB_TOPIC_STR = MQTT_PRODUCTINFO_PRO;
        PUB_TOPIC_STR += BLINKER_F("/");
        PUB_TOPIC_STR += _userID;
        PUB_TOPIC_STR += BLINKER_F("/s");

        BLINKER_PUB_TOPIC_PRO = (char*)malloc((PUB_TOPIC_STR.length() + 1)*sizeof(char));
        // memcpy(BLINKER_PUB_TOPIC_PRO, PUB_TOPIC_STR.c_str(), str_len);
        strcpy(BLINKER_PUB_TOPIC_PRO, PUB_TOPIC_STR.c_str());
        
        BLINKER_LOG_ALL(BLINKER_F("BLINKER_PUB_TOPIC_PRO: "), BLINKER_PUB_TOPIC_PRO);
        
        String SUB_TOPIC_STR = MQTT_PRODUCTINFO_PRO;
        SUB_TOPIC_STR += BLINKER_F("/");
        SUB_TOPIC_STR += _userID;
        SUB_TOPIC_STR += BLINKER_F("/r");
        
        BLINKER_SUB_TOPIC_PRO = (char*)malloc((SUB_TOPIC_STR.length() + 1)*sizeof(char));
        // memcpy(BLINKER_SUB_TOPIC_PRO, SUB_TOPIC_STR.c_str(), str_len);
        strcpy(BLINKER_SUB_TOPIC_PRO, SUB_TOPIC_STR.c_str());
        
        BLINKER_LOG_ALL(BLINKER_F("BLINKER_SUB_TOPIC_PRO: "), BLINKER_SUB_TOPIC_PRO);
    }
    else if (_broker == BLINKER_MQTT_BORKER_ONENET) {
        uint8_t str_len;
        String PUB_TOPIC_STR = MQTT_PRODUCTINFO_PRO;
        PUB_TOPIC_STR += BLINKER_F("/onenet_rule/r");
        // str_len = PUB_TOPIC_STR.length() + 1;
        BLINKER_PUB_TOPIC_PRO = (char*)malloc((PUB_TOPIC_STR.length() + 1)*sizeof(char));
        // memcpy(BLINKER_PUB_TOPIC_PRO, PUB_TOPIC_STR.c_str(), str_len);
        strcpy(BLINKER_PUB_TOPIC_PRO, PUB_TOPIC_STR.c_str());
        
        BLINKER_LOG_ALL(BLINKER_F("BLINKER_PUB_TOPIC_PRO: "), BLINKER_PUB_TOPIC_PRO);
        
        String SUB_TOPIC_STR = MQTT_PRODUCTINFO_PRO;
        SUB_TOPIC_STR += BLINKER_F("/");
        SUB_TOPIC_STR += _userID;
        SUB_TOPIC_STR += BLINKER_F("/r");
        
        BLINKER_SUB_TOPIC_PRO = (char*)malloc((SUB_TOPIC_STR.length() + 1)*sizeof(char));
        // memcpy(BLINKER_SUB_TOPIC_PRO, SUB_TOPIC_STR.c_str(), str_len);
        strcpy(BLINKER_SUB_TOPIC_PRO, SUB_TOPIC_STR.c_str());
        
        BLINKER_LOG_ALL(BLINKER_F("BLINKER_SUB_TOPIC_PRO: "), BLINKER_SUB_TOPIC_PRO);
    }

    // BLINKER_LOG_FreeHeap();

    if (_broker == BLINKER_MQTT_BORKER_ALIYUN) {
        #if defined(ESP8266)
            // bool mfln = client_pro.probeMaxFragmentLength(MQTT_HOST_PRO, MQTT_PORT_PRO, 4096);
            // if (mfln) {
            //     client_pro.setBufferSizes(1024, 1024);
            // }
            // client_pro.setInsecure();
            mqtt_PRO = new Adafruit_MQTT_Client(&client_pro, MQTT_HOST_PRO, MQTT_PORT_PRO, MQTT_ID_PRO, MQTT_NAME_PRO, MQTT_KEY_PRO);
        #elif defined(ESP32)
            mqtt_PRO = new Adafruit_MQTT_Client(&client_s_PRO, MQTT_HOST_PRO, MQTT_PORT_PRO, MQTT_ID_PRO, MQTT_NAME_PRO, MQTT_KEY_PRO);
        #endif
    }
    else if (_broker == BLINKER_MQTT_BORKER_QCLOUD) {
        #if defined(ESP8266)
            // bool mfln = client_pro.probeMaxFragmentLength(MQTT_HOST_PRO, MQTT_PORT_PRO, 4096);
            // if (mfln) {
            //     client_pro.setBufferSizes(1024, 1024);
            // }
            // client_pro.setInsecure();
            mqtt_PRO = new Adafruit_MQTT_Client(&client_pro, MQTT_HOST_PRO, MQTT_PORT_PRO, MQTT_ID_PRO, MQTT_NAME_PRO, MQTT_KEY_PRO);
        #elif defined(ESP32)
            mqtt_PRO = new Adafruit_MQTT_Client(&client_s_PRO, MQTT_HOST_PRO, MQTT_PORT_PRO, MQTT_ID_PRO, MQTT_NAME_PRO, MQTT_KEY_PRO);
        #endif
    }
    else if (_broker == BLINKER_MQTT_BORKER_ONENET) {
        mqtt_PRO = new Adafruit_MQTT_Client(&client_PRO, MQTT_HOST_PRO, MQTT_PORT_PRO, MQTT_ID_PRO, MQTT_NAME_PRO, MQTT_KEY_PRO);
    }

    // iotPub = new Adafruit_MQTT_Publish(mqtt_PRO, BLINKER_PUB_TOPIC_PRO);
    iotSub_PRO = new Adafruit_MQTT_Subscribe(mqtt_PRO, BLINKER_SUB_TOPIC_PRO);

    // mqtt_broker = (char*)malloc((_broker.length()+1)*sizeof(char));
    // strcpy(mqtt_broker, _broker.c_str());
    // mqtt_broker = _broker;

    // mDNSInit(MQTT_ID_PRO);
    this->latestTime = millis();
    mqtt_PRO->subscribe(iotSub_PRO);
    isMQTTinit = true;
    connect();

    return true;
}

void BlinkerPRO::mDNSInit()
{
#if defined(ESP8266)
    if (!MDNS.begin(macDeviceName().c_str(), WiFi.localIP())) {
#elif defined(ESP32)
    if (!MDNS.begin(macDeviceName().c_str())) {
#endif
        while(1) {
            ::delay(100);
        }
    }

    BLINKER_LOG(BLINKER_F("mDNS responder started"));
    
    String _service = STRING_format(BLINKER_MDNS_SERVICE_BLINKER) + _deviceType;
            
    MDNS.addService(_service.c_str(), "tcp", WS_SERVERPORT);
    MDNS.addServiceTxt(_service.c_str(), "tcp", "deviceName", macDeviceName());

    webSocket_PRO.begin();
    webSocket_PRO.onEvent(webSocketEvent_PRO);
    BLINKER_LOG(BLINKER_F("webSocket_PRO server started"));
    BLINKER_LOG(BLINKER_F("ws://"), macDeviceName(), BLINKER_F(".local:"), WS_SERVERPORT);
}

void BlinkerPRO::checkKA() {
    if (millis() - kaTime >= BLINKER_MQTT_KEEPALIVE)
        isAlive = false;
}

bool BlinkerPRO::checkAliKA() {
    if (millis() - aliKaTime >= 10000)
        return false;
    else
        return true;
}

bool BlinkerPRO::checkCanPrint() {
    if ((millis() - printTime >= BLINKER_MQTT_MSG_LIMIT && isAlive) || printTime == 0) {
        return true;
    }
    else {
        BLINKER_ERR_LOG_ALL(BLINKER_F("MQTT NOT ALIVE OR MSG LIMIT"));
        
        checkKA();

        return false;
    }
}

bool BlinkerPRO::checkCanBprint() {
    if ((millis() - bPrintTime >= BLINKER_BRIDGE_MSG_LIMIT) || bPrintTime == 0) {
        return true;
    }
    else {
        BLINKER_ERR_LOG_ALL(BLINKER_F("MQTT NOT ALIVE OR MSG LIMIT"));
        
        return false;
    }
}

bool BlinkerPRO::checkPrintSpan() {
    if (millis() - respTime < BLINKER_PRINT_MSG_LIMIT) {
        if (respTimes > BLINKER_PRINT_MSG_LIMIT) {
            BLINKER_ERR_LOG_ALL(BLINKER_F("WEBSOCKETS CLIENT NOT ALIVE OR MSG LIMIT"));
            
            return false;
        }
        else {
            respTimes++;
            return true;
        }
    }
    else {
        respTimes = 0;
        return true;
    }
}

bool BlinkerPRO::checkAliPrintSpan()
{
    if (millis() - respAliTime < BLINKER_PRINT_MSG_LIMIT/2)
    {
        if (respAliTimes > BLINKER_PRINT_MSG_LIMIT/2)
        {
            BLINKER_ERR_LOG_ALL(BLINKER_F("ALIGENIE NOT ALIVE OR MSG LIMIT"));
            
            return false;
        }
        else
        {
            respAliTimes++;
            return true;
        }
    }
    else
    {
        respAliTimes = 0;
        return true;
    }
}

bool BlinkerPRO::pubHello()
{
    String stateJsonStr = BLINKER_F("{\"message\":\"Registration successful\"}");
    
    BLINKER_LOG_ALL(BLINKER_F("PUB hello: "), stateJsonStr);
    
    return print("{\"message\":\"Registration successful\"}");
}

bool BlinkerPRO::isJson(const String & data)
{
    DynamicJsonBuffer jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(STRING_format(data));

    if (!root.success())
    {
        BLINKER_ERR_LOG("Not a Json data!");
        return false;
    }

    return true;
}

#endif