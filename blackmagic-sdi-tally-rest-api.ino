#include <SPI.h>
#include <Ethernet.h>
#include <BMDSDIControl.h>

BMD_SDITallyControl_I2C sdiTallyControl(0x6E);            // define the Tally object using I2C using the default shield address

/*
  MACRO for string handling from PROGMEM
  https://todbot.com/blog/2008/06/19/how-to-do-big-strings-in-arduino/
  max 149 chars at once ...
*/
char p_buffer[150];
#define P(str) (strcpy_P(p_buffer, PSTR(str)), p_buffer)

// Ethernet Interface Settings
byte mac[] =      { 0xAD, 0xDE, 0xEF, 0xBB, 0xFD, 0xDD };
IPAddress ip      (172, 17, 121, 64);  // Private static IP address
//IPAddress myDns   (192, 168, 2, 1);   // DNS is not needed for this example
//IPAddress gateway (0, 0, 0, 0);       // Default gateway disabled for security reasons
IPAddress subnet  (255, 255, 255, 0); // Class C subnet; typical

// HTTP lives on TCP port 80
EthernetServer server(80);

void setup()
{
  // start the Ethernet connection and the server:
  Ethernet.begin(mac, ip);
  server.begin();
  Serial.begin(9600);

  // SDI
  sdiTallyControl.begin();                                 // initialize tally control
  sdiTallyControl.setOverride(true);                       // enable tally override
}

// string buffers for receiving URL and arguments
char bufferUrl[256];
char bufferArgs[512];
int urlChars = 0;
int argChars = 0;

// number of characters read on the current line
int lineChars = 0;

// connection state while receiving a request
int state = 0;

/*
  Typical request: GET /<request goes here>?firstArg=1&anotherArg=2 HTTP/1.1
  State 0 - connection opened
  State 1 - receiving URL
  State 2 - receiving Arguments
  State 3 - arguments and/or URL finished
  State 4 - client has ended request, waiting for server to respond
  State 5 - server has responded
  
  Example of what the server receives:
  
  GET /test.html HTTP/1.1
  Host: 192.168.1.23
  Connection: keep-alive
  Cache-Control: max-age=0
  Upgrade-Insecure-Requests: 1
  User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/58.0.3029.110 Safari/537.36
  Accept-Encoding: gzip, deflate, sdch
  Accept-Language: en-US,en;q=0.8
*/

void loop()
{
  // listen for incoming clients
  EthernetClient client = server.available();
  
  if (client) 
  {
    state = 0;
    urlChars = 0;
    argChars = 0;
    lineChars = 0;
    bufferUrl[0] = 0;
    bufferArgs[0] = 0;

    while (client.connected()) 
    {
      if (client.available()) 
      {
        // read and echo data received from the client
        char c = client.read();
        Serial.print(c);

        // ignore \r carriage returns, we only care about \n new lines
        if (c == '\r')
          continue;        
          
        // control what happens to the buffer:
        if (state == 0 && c == '/')
        {
          // Begin receiving URL
          state = 1; 
        }
        else if (state == 1 && c == '?')
        {
          // Begin receiving args
          state = 2;
        }
        else if ((state == 1 || state == 2) && c == ' ')
        {
          // Received full request URL and/or args
          state = 3;
        }
        else if (state == 1 && urlChars < 255)
        {
            // Receiving URL (allow up to 255 chars + null terminator)
            bufferUrl[urlChars++] = c;
            bufferUrl[urlChars] = 0;
        }
        else if (state == 2 && argChars < 511)
        {
            // Receiving Args (allow up to 511 chars + null terminator)
            bufferArgs[argChars++] = c;
            bufferArgs[argChars] = 0;
        }
        else if (state == 3 && c == '\n' && lineChars == 0)
        {
          // Received a line with no characters; this means the client has ended their request
          state = 4;
        }

        // record how many characters on the line so far:
        if (c == '\n')
          lineChars = 0;
        else
          lineChars++;

        // OK to respond
        if(state == 4)
        {
          // Response given
          state = 5; 

          Serial.print(P(": "));
          Serial.println(bufferUrl);

          // handle the response
          respond(client);

          // exit the loop
          break;
        }
      }
    }
    
    // flush and close the connection:
    client.flush();
    client.stop();
  }
}

void respond(EthernetClient client)
{
  if (strcmp(bufferUrl, P("")) == 0)
  {
    // Requested: /  (DEFAULT PAGE)
    
    // send response header
    sendHttpResponseOk(client);

    // send html page
    // max length:    -----------------------------------------------------------------------------------------------------------------------------------------------------  (149 chars)
    client.println(P("<HTML><head><title>Blackmagic SDI Tally REST API</title></head><body><h1>Blackmagic SDI Tally REST API</h1>"));
    client.println(P("<p>HTTP Bridge for the Blackmagic Arduino Shield to embed SDI Tally Metadata into an SDI Signal</p>"));
    
    client.println(P("<h2>Control Tally Preview and Program on 4 cameras</h2>"));
    client.println(P("<ul>"));
    client.println(P("<li><a href=\"tally?cam=1&pgm=1&pvw=1\">?cam=1&pgm=1&pvw=1</a></li>"));
    client.println(P("</ul>"));
    
    client.println(P("<p>Get full documentation at <a href=\"http://github.com/airbenich/blackmagic-sdi-tally-rest-api\">Github</a><p>"));
    client.println(P("</body></html>"));
  }
  else if (strcmp(bufferUrl, P("tally")) == 0)
  {
    // Requested: tally

    //  sdiTallyControl.setCameraTally(                          // turn tally ON
    //    1,                                                     // Camera Number
    //    true,                                                  // Program Tally
    //    false                                                  // Preview Tally
    //  );

    //?cam=1&pgm=1&pvw=1
    
    // send response header
    sendJsonResponseOk(client);

    int camera = 1;
    bool program = false;
    bool preview = false;
    
    // get camera
    if(strstr(bufferArgs, P("cam=1"))) {
      camera = 1;
    } else if(strstr(bufferArgs, P("cam=2"))) {
      camera = 2;
    } else if(strstr(bufferArgs, P("cam=3"))) {
      camera = 3;
    } else if(strstr(bufferArgs, P("cam=4"))) {
      camera = 4;
    }

    // get program parameter
    if(strstr(bufferArgs, P("pgm=1"))) {
      program = true;
    }

    // get preview parameter
    if(strstr(bufferArgs, P("pvw=1"))) {
      preview = true;
    }

    // set Tally
    sdiTallyControl.setCameraTally(camera, program, preview);

    // send response
    client.print("{");
    client.print("\"camera\": ");
    client.print(camera);
    client.print(", ");
    client.print("\"program\": ");
    client.print(program);
    client.print(", ");
    client.print("\"preview\": ");
    client.print(preview);
    client.print("}");
  }
  else
  {
    // All other requests - 404 not found
    
    // send 404 not found header (oops)
    sendHttp404(client);
    client.println(P("<HTML><head><title>Resource not found</title></head><body><h1>The requested resource was not found</h1>"));
    client.println(P("<br><b>Resource:</b> "));
    client.println(bufferUrl);
    client.println(P("<br><b>Arguments:</b> "));
    client.println(bufferArgs);
    client.println(P("</body></html>"));
  }
}

// 200 OK means the resource was located on the server and the browser (or service consumer) should expect a happy response
void sendHttpResponseOk(EthernetClient client)
{
  Serial.println(P("200 OK"));
  Serial.println();
  
  // send a standard http response header
  client.println(P("HTTP/1.1 200 OK"));
  client.println(P("Content-Type: text/html"));
  client.println(P("Connnection: close")); // do not reuse connection
  client.println();
}

// 200 OK means the resource was located on the server and the browser (or service consumer) should expect a happy response
void sendJsonResponseOk(EthernetClient client)
{
  Serial.println(P("200 OK"));
  Serial.println();
  
  // send a standard http response header
  client.println(P("HTTP/1.1 200 OK"));
  client.println(P("Content-Type: application/json"));
  client.println(P("Connnection: close")); // do not reuse connection
  client.println();
}

// 404 means it ain't here. quit asking.
void sendHttp404(EthernetClient client)
{
  Serial.println(P("404 Not Found"));
  Serial.println();
  
  client.println(P("HTTP/1.1 404 Not Found"));
  client.println(P("Content-Type: text/html"));
  client.println(P("Connnection: close")); // do not reuse connection
  client.println();
}
