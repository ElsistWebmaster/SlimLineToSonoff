// *****************************************************************************
// Project    : MDP126A100
// Programmer : Sergio Bertana
// Date       : 14/02/2018
// *****************************************************************************
// Gestione disposistivo Sonoff.
// Il valore ritornato da millis esegue overlap dopo circa 50 giorni.
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// ESEGUO DEFINIZIONI GENERALI
// -----------------------------------------------------------------------------
// Includo definizioni generali.

#include <ESP8266WiFi.h>

// Definizione versione firmware.

#define FIRMWAREVERSION "MDP126A100"

// Definizione segnali hardware dispositivo.

#define BUTTON 0 //GPIO0, Button
#define RELAY_PIN 12 //GPIO12, Relay
#define GREEN_LED 13 //GPIO13, Green led
#define RED_LED 14 //GPIO14, Red led

// Definizione comandi.

#define STATUS_REQUEST 1 //Richiesta status
#define RELAY_COMMAND 2 //Comando relè
#define SYSTEM_PROMPT 100 //Invio prompt

// -----------------------------------------------------------------------------
// CONFIGURAZIONE RETE
// -----------------------------------------------------------------------------
// Configurazione WiFi.

const char* SSID="My SSID"; //Name of your network (SSID)
const char* Password="My Password"; //WiFi password 

// Configurazione rete.

IPAddress IP(192,168,0,183); //Node static IP
IPAddress Gateway(192,168,0,1); //Gateway IP
IPAddress Subnet(255,255,255,0); //Subnet mask

// Definizione porta Server.

WiFiServer Server(1000); //Initialize the server library
WiFiClient Client; //Initialize the client library

// -----------------------------------------------------------------------------
// DEFINIZIONE VARIABILI GLOBALI
// -----------------------------------------------------------------------------
// Definizione variabili globali

bool RelaySts; //Relay status
bool ButtonSts; //Button status
char Request[80]; //Request data buffer
char Buffer[80]; //sprintf buffer
unsigned long CmdTmBf; //Command time buffer (mS)
unsigned long WiFiEr; //WiFi errors

// *****************************************************************************
// INIT PROGRAM
// *****************************************************************************
// Programma eseguito alla accensione.
// -----------------------------------------------------------------------------

void setup() 
{
    // -------------------------------------------------------------------------
    // IMPOSTAZIONE PIN GPIO
    // -------------------------------------------------------------------------
    // Impostazione funzioni su pin GPIO.

    pinMode(GREEN_LED, OUTPUT); //Imposto pin come output
    pinMode(RELAY_PIN, OUTPUT); //Imposto pin come output
    pinMode(RED_LED, OUTPUT); //Imposto pin come output 
    pinMode(BUTTON, INPUT); //Imposto pin come input   

    // Setto stato di default sui pin di GPIO.
          
    digitalWrite(GREEN_LED, HIGH); //Accendo LED
    digitalWrite(RELAY_PIN, LOW); //Spengo relè
    digitalWrite(RED_LED, HIGH); //Accendo LED
    digitalWrite(BUTTON, HIGH); //Predispongo acquisizione pulsante

    // -------------------------------------------------------------------------
    // ATTIVAZIONE SERVER SERIALE
    // -------------------------------------------------------------------------
    // Attivo server seriale.
 
    Serial.begin(9600); //9600, n, 8

    // Eseguo report su seriale.

    Serial.print("\r\nElsist Sonoff firmware: "FIRMWAREVERSION"\r\n");
    Serial.print("Start configuration\r\n");     

    // -------------------------------------------------------------------------
    // ATTIVAZIONE WIFI
    // -------------------------------------------------------------------------
    // Eseguo attivazione WiFi.
    
    WiFi.mode (WIFI_STA); //Imposto modalità station   
    delay(100); //Attesa 100 mS
    WiFi.begin(SSID, Password, IP); //Aggancio WiFi alla rete
    WiFi.config(IP, Gateway, Subnet); //Imposto parametri di rete
    CmdTmBf=millis(); //Command time buffer (mS)

    // Eseguo attesa connessione alla rete.
 
    while (WiFi.status() != WL_CONNECTED) {delay(500); Serial.print(".");}
    Serial.print("\r\nWiFi connected\r\n");

    // Gestione server.

    Server.begin();
    Serial.print("Server start, system IP:\r\n");
    Serial.println(WiFi.localIP());
}    

// *****************************************************************************
// MAIN PROGRAM LOOP
// *****************************************************************************
// Program loop eseguito ciclicamente.
// -----------------------------------------------------------------------------

void loop() 
{
    // -------------------------------------------------------------------------
    // DEFINIZIONE VARIABILI
    // -------------------------------------------------------------------------
    // Definizione variabili statiche.
    
    static bool IsConnect; //Client is connect
    static bool LEDStatus; //LED status
    static char RxIDx; //Rx index
    static char CType=SYSTEM_PROMPT; //Command type
    static unsigned long LEDTmBf; //LED time buffer (mS)

    // -------------------------------------------------------------------------
    // CONTROLLO WIFI
    // -------------------------------------------------------------------------
    // Controllo se ricevuto comandi in un minuto.

    if ((millis()-CmdTmBf) > 60000)
    {
        CmdTmBf=millis(); //Command time buffer (mS)
        WiFiEr++; //WiFi errors
        sprintf(Buffer, "WiFi errors: %d\r\n", WiFiEr);
        Serial.print(Buffer);
        Client.stop();
        Server.stop();
        WiFi.begin(); //Aggancio WiFi alla rete
        Server.begin();
    }     

    // -------------------------------------------------------------------------
    // GESTIONE LAMPEGGIO LED
    // -------------------------------------------------------------------------
    // Viene eseguito il lampeggio del led.

    if ((millis()-LEDTmBf) > ((!LEDStatus)?2000:200))
    {
        LEDTmBf=millis(); //LED time buffer (mS)
        (LEDStatus)?LEDStatus=false:LEDStatus=true;
    } 

    // Il comando del LED và invertito (true:Spegne, false: accende).
    
    digitalWrite(GREEN_LED, (RelaySts)?LEDStatus:!LEDStatus);

    // -------------------------------------------------------------------------
    // ACQUISIZIONE PULSANTE E GESTIONE RELE'
    // -------------------------------------------------------------------------
    // Eseguo acquisizione pulsante, lo stato và invertito.

    ButtonSts=!digitalRead(BUTTON); //Button status
    digitalWrite(RELAY_PIN, RelaySts); //Relay command
    
    // -------------------------------------------------------------------------
    // CONTROLLO CONNESSIONE CLIENT
    // -------------------------------------------------------------------------
    // Controllo se Client connesso.

    if (!IsConnect)
    {
        Client=Server.available(); //Client
        if (Client)
        {
            CType=SYSTEM_PROMPT; //Command type
            IsConnect=true; //Client is connect
            Serial.print("Client connected\r\n");
            Client.print("Elsist Sonoff firmware: "FIRMWAREVERSION"\r\nWelcome...\r\n");
        }
    }
    else
    {
        if (!Client.connected())
        {
            IsConnect=false; //Client is connect
            Serial.print("Client disconnected\r\n");
        }
    }
    
    // -------------------------------------------------------------------------
    // GESTIONE COMANDI
    // -------------------------------------------------------------------------
    // Gestione comandi ricevuti.

    if (!IsConnect) return; //Nessun client connesso

    // Eseguo selezione comando ricevuto.
  
    switch (CType)
    {
        case SYSTEM_PROMPT: Client.print("> "); CType=0; RxIDx=0;  break;
        case STATUS_REQUEST: if (StatusRequest(false)) CType=SYSTEM_PROMPT; return; //Richiesta status
        case RELAY_COMMAND: if (RelayCommand(false)) CType=SYSTEM_PROMPT; return; //Comando relè
    }

    // -------------------------------------------------------------------------
    // GESTIONE PROTOCOLLO
    // -------------------------------------------------------------------------
    // available, get the number of bytes (characters) available for reading.
    // This is data that's already arrived and stored in the receive buffer
    // (which holds 64 bytes).
    // -------------------------------------------------------------------------
     // Controllo se caratteri ricevuti dal client. 

    while (Client.available() > 0)
    {
        // Eseguo lettura caratteri e li salvo in buffer.

        Request[RxIDx]=Client.read(); //Carattere letto
        Request[RxIDx+1]=0; //Codice tappo
        if (++RxIDx > 40) RxIDx=0; //Rx index

        // Eseguo controllo se ricevuto <CR>.

        if (Request[RxIDx-1] == 13)
        {
            CmdTmBf=millis(); //Command time buffer (mS)
            Client.print(Request);

            // Eseguo controllo comando ricevuto.

            if (strstr(Request, "Status") > 0) {CType=STATUS_REQUEST; StatusRequest(true);}
            if (strstr(Request, "Relay") > 0) {CType=RELAY_COMMAND; RelayCommand(true);}

            // Se comando non riconosciuto output prompt.

            if (!CType)
            {
                Client.print("Command mismatch...\r\n");
                CType=SYSTEM_PROMPT; //Command type
            }
        }
    }
}

// *****************************************************************************
// GESTIONE COMANDO RICHIESTA STATO
// *****************************************************************************
// Ritorna lo stato del dispositivo.
// -----------------------------------------------------------------------------

bool StatusRequest(bool Init) 
{
    // -------------------------------------------------------------------------
    // DEFINIZIONE VARIABILI
    // -------------------------------------------------------------------------
    // Definizione variabili statiche.
    
    static unsigned char CaseNr; //Program case
    static long RSSIValue; //Valore Rssi
   
    // -------------------------------------------------------------------------
    // GESTIONE INIZIALIZZAZIONE
    // -------------------------------------------------------------------------
    // Gestione inizializzazione.

    if (Init) {CaseNr=0; return(false);}

    // -------------------------------------------------------------------------
    // GESTIONE CASES COMANDO
    // -------------------------------------------------------------------------
    // Gestione cases comando.

    switch (CaseNr++)
    {
      case 0: RSSIValue=WiFi.RSSI(); return(false);
      case 1: sprintf(Buffer, "RSSI value: %d\r\n", RSSIValue); Client.print(Buffer); return(false);
      case 2: sprintf(Buffer, "WiFi errors: %d\r\n", WiFiEr); Client.print(Buffer); return(false);
      case 3: sprintf(Buffer, "Button is: %s\r\n", (!ButtonSts)?"Off":"On"); Client.print(Buffer); return(false);
      case 4: sprintf(Buffer, "Relay is: %s\r\n", (!RelaySts)?"Off":"On"); Client.print(Buffer); return(false);
      default: return(true);
    }
}

// *****************************************************************************
// GESTIONE COMANDO ATTIVAZIONE RELE'
// *****************************************************************************
// Gestisce il comando del relè.
// -----------------------------------------------------------------------------

bool RelayCommand(bool Init) 
{
    // -------------------------------------------------------------------------
    // GESTIONE COMANDO RELE'
    // -------------------------------------------------------------------------
    // Gestione comando relè.

    if (Init)
    {
        if (strstr(Request, "Off") > 0) RelaySts=false;
        if (strstr(Request, "On") > 0) RelaySts=true;
        return(false);
    }
 
    // -------------------------------------------------------------------------
    // GESTIONE RISPOSTA
    // -------------------------------------------------------------------------
    // Gestione risposta al comando.
          
    sprintf(Buffer, "Button is: %s, Relay is: %s\r\n", (!ButtonSts)?"Off":"On", (!RelaySts)?"Off":"On");
    Client.print(Buffer);
    return(true);
 }

