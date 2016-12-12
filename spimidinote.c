/*
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//the midinote.c, midiprogramchange.c and midicontinuouscontroller.c functions are portmidi equivalents of
//the midinote.jar, midiprogramchange.jar and midicontinuouscontroller.jar java class archives which are java sound midi equivalents of
//the midi_autohotkey-midi-function_example-note-on-note-off.ahk, midi_autohotkey-midi-function_example-program-change.ahk and midi_autohotkey-midi-function_example-continuous-controller.ahk 
//which are based on autohotkey.exe script midi library midi_autohotkey-midi-functions.ahk (that can be found in folder http://www.oifii.org/ns-org/nsd/ar/cp/midi_autohotkey/)
//
//portmidi, portable c midi library compiled with this project can be found in folder http://www.oifii.org/ns-org/nsd/ar/cp/midi_portmidi-src-217/ 
//
//java sound midi class archives equivalent examples (midinote.jar, midiprogramchange.jar and midicontinuouscontroller.jar) can be found in folders:
//http://www.oifii.org/ns-org/nsd/ar/cp/midi_java-sound-midi_midinote/
//http://www.oifii.org/ns-org/nsd/ar/cp/midi_java-sound-midi_midiprogramchange/
//http://www.oifii.org/ns-org/nsd/ar/cp/midi_java-sound-midi_midicontinuouscontroller/
//http://www.oifii.org/ns-org/nsd/ar/cp/midi_java-sound-midi_midicontinuouscontrollerraw/
//
//portmidi based midinote.c, midiprogramchange.c and midicontinuouscontroller.c can be found in folder:
//http://www.oifii.org/ns-org/nsd/ar/cp/midi_portmidi-src-217_midinote/
//http://www.oifii.org/ns-org/nsd/ar/cp/midi_portmidi-src-217_midiprogramchange/
//http://www.oifii.org/ns-org/nsd/ar/cp/midi_portmidi-src-217_midicontinuouscontroller/
//
//
//2011dec10, version 0.2, spi, revised to properly implement http://www.midi.org/techspecs/midimessages.php, in that table channel id is a value between 0 and 15 like inside this code but
//							   new main program interface parameters implements channel id for user with integer value between 1 and 16
//							   tests: when closing mididevice, opened with non-zero latency, cc 64 value 0 are sent on all channels
//							   default latency is zero if not specified, no undesirable cc 64 values will be sent
//							   -c option does not work for the time being, on exit always closes mididevice, this because atexit(exit) is called in pminit(), see pmwin.c, don't know the work around for now
//2011oct23, version 0.1, spi, created stephane.poirier@nakedsoftware.org
//
//2014may04, version 0.2, spi, renamed spimidinote.c
//
//copyright 2012, stephane.poirier@nakedsoftware.org
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
*/

#include "portmidi.h"
#include "porttime.h"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"
#include "assert.h"
#include "math.h" //i.e. for min() and max() function calls
#include "windows.h" //i.e. for Sleep() function calls


#define INPUT_BUFFER_SIZE 100
#define OUTPUT_BUFFER_SIZE 0
#define DRIVER_INFO NULL
#define TIME_PROC ((int32_t (*)(void *)) Pt_Time)
#define TIME_INFO NULL
#define TIME_START Pt_Start(1, 0, 0) /* timer started w/millisecond accuracy */

#define STRING_MAX 80 /* used for console input */

int32_t latency = 0;


/* from oifii.org's autohotkey midi library, but adapted for channel id between 0 and 15
midiOutShortMsg(h_midiout, EventType, Channel, Param1, Param2) 
{ 
  ;h_midiout is handle to midi output device returned by midiOutOpen function 
  ;EventType and Channel are combined to create the MidiStatus byte.  
  ;MidiStatus message table can be found at http://www.midi.org/techspecs/midimessages.php 
  ;Possible values for EventTypes are NoteOn (N1), NoteOff (N0), CC, PolyAT (PA), ChanAT (AT), PChange (PC), Wheel (W) - vals in () are optional shorthand 
  ;SysEx not supported by the midiOutShortMsg call 
  ;Param3 should be 0 for PChange, ChanAT, or Wheel.  When sending Wheel events, put the entire Wheel value 
  ;in Param2 - the function will split it into it's two bytes 
  ;returns 0 if successful, -1 if not.  
  
  ;Calc MidiStatus byte combining a channel number between 0 and 15
  If (EventType = "NoteOn" OR EventType = "N1") 
    MidiStatus :=  144 + Channel 
  Else if (EventType = "NoteOff" OR EventType = "N0") 
    MidiStatus := 128 + Channel 
  Else if (EventType = "NoteOffAll") 
    ;MidiStatus := 124 + Channel
    MidiStatus := 176 + Channel
  Else if (EventType = "CC") 
    MidiStatus := 176 + Channel 
  Else if (EventType = "PolyAT" OR EventType = "PA") 
    MidiStatus := 160 + Channel 
  Else if (EventType = "ChanAT" OR EventType = "AT") 
    MidiStatus := 208 + Channel 
  Else if (EventType = "PChange" OR EventType = "PC") 
    MidiStatus := 192 + Channel 
  Else if (EventType = "Wheel" OR EventType = "W") 
  {  
    MidiStatus := 224 + Channel 
    Param2 := Param1 >> 8 ;MSB of wheel value 
    Param1 := Param1 & 0x00FF ;strip MSB, leave LSB only 
  } 

  ;Midi message Dword is made up of Midi Status in lowest byte, then 1st parameter, then 2nd parameter.  Highest byte is always 0 
  dwMidi := MidiStatus + (Param1 << 8) + (Param2 << 16) 
*/


/* read a number from console */
/**/
int get_number(char *prompt)
{
    char line[STRING_MAX];
    int n = 0, i;
    printf(prompt);
    while (n != 1) {
        n = scanf("%d", &i);
        fgets(line, STRING_MAX, stdin);

    }
    return i;
}





void main_test_output() {
    PmStream * midi;
	char line[80];
    int32_t off_time;
    int chord[] = { 60, 67, 76, 83, 90 };
    #define chord_size 5 
    PmEvent buffer[chord_size];
    PmTimestamp timestamp;

    /* determine which output device to use */
    int i = get_number("Type output number: ");

    /* It is recommended to start timer before PortMidi */
    TIME_START;

    /* open output device -- since PortMidi avoids opening a timer
       when latency is zero, we will pass in a NULL timer pointer
       for that case. If PortMidi tries to access the time_proc,
       we will crash, so this test will tell us something. */
    Pm_OpenOutput(&midi, 
                  i, 
                  DRIVER_INFO,
                  OUTPUT_BUFFER_SIZE, 
                  (latency == 0 ? NULL : TIME_PROC),
                  (latency == 0 ? NULL : TIME_INFO), 
                  latency);
    printf("Midi Output opened with %ld ms latency.\n", (long) latency);

    /* output note on/off w/latency offset; hold until user prompts */
    printf("ready to send program 1 change... (type RETURN):");
    fgets(line, STRING_MAX, stdin);
    /* if we were writing midi for immediate output, we could always use
       timestamps of zero, but since we may be writing with latency, we
       will explicitly set the timestamp to "now" by getting the time.
       The source of timestamps should always correspond to the TIME_PROC
       and TIME_INFO parameters used in Pm_OpenOutput(). */
    buffer[0].timestamp = TIME_PROC(TIME_INFO);
    /* Send a program change to increase the chances we will hear notes */
    /* Program 0 is usually a piano, but you can change it here: */
#define PROGRAM 0
    buffer[0].message = Pm_Message(0xC0, PROGRAM, 0);
    Pm_Write(midi, buffer, 1);

    printf("ready to note-on... (type RETURN):");
    fgets(line, STRING_MAX, stdin);
    buffer[0].timestamp = TIME_PROC(TIME_INFO);
    buffer[0].message = Pm_Message(0x90, 60, 100);
    Pm_Write(midi, buffer, 1);
    printf("ready to note-off... (type RETURN):");
    fgets(line, STRING_MAX, stdin);
    buffer[0].timestamp = TIME_PROC(TIME_INFO);
    buffer[0].message = Pm_Message(0x90, 60, 0);
    Pm_Write(midi, buffer, 1);

    /* output short note on/off w/latency offset; hold until user prompts */
    printf("ready to note-on (short form)... (type RETURN):");
    fgets(line, STRING_MAX, stdin);
    Pm_WriteShort(midi, TIME_PROC(TIME_INFO),
                  Pm_Message(0x90, 60, 100));
    printf("ready to note-off (short form)... (type RETURN):");
    fgets(line, STRING_MAX, stdin);
    Pm_WriteShort(midi, TIME_PROC(TIME_INFO),
                  Pm_Message(0x90, 60, 0));

    /* output several note on/offs to test timing. 
       Should be 1s between notes */
    printf("chord will arpeggiate if latency > 0\n");
    printf("ready to chord-on/chord-off... (type RETURN):");
    fgets(line, STRING_MAX, stdin);
    timestamp = TIME_PROC(TIME_INFO);
    for (i = 0; i < chord_size; i++) {
        buffer[i].timestamp = timestamp + 1000 * i;
        buffer[i].message = Pm_Message(0x90, chord[i], 100);
    }
    Pm_Write(midi, buffer, chord_size);

    off_time = timestamp + 1000 + chord_size * 1000; 
    while (TIME_PROC(TIME_INFO) < off_time) 
		/* busy wait */;
    for (i = 0; i < chord_size; i++) {
        buffer[i].timestamp = timestamp + 1000 * i;
        buffer[i].message = Pm_Message(0x90, chord[i], 0);
    }
    Pm_Write(midi, buffer, chord_size);    

    /* close device (this not explicitly needed in most implementations) */
    printf("ready to close and terminate... (type RETURN):");
    fgets(line, STRING_MAX, stdin);
	
    Pm_Close(midi);
    Pm_Terminate();
    printf("done closing and terminating...\n");
}


void main_test_both()
{
    int i = 0;
    int in, out;
    PmStream * midi, * midiOut;
    PmEvent buffer[1];
    PmError status, length;
    int num = 10;
    
    in = get_number("Type input number: ");
    out = get_number("Type output number: ");

    /* In is recommended to start timer before PortMidi */
    TIME_START;

    Pm_OpenOutput(&midiOut, 
                  out, 
                  DRIVER_INFO,
                  OUTPUT_BUFFER_SIZE, 
                  TIME_PROC,
                  TIME_INFO, 
                  latency);
    printf("Midi Output opened with %ld ms latency.\n", (long) latency);
    /* open input device */
    Pm_OpenInput(&midi, 
                 in,
                 DRIVER_INFO, 
                 INPUT_BUFFER_SIZE, 
                 TIME_PROC, 
                 TIME_INFO);
    printf("Midi Input opened. Reading %d Midi messages...\n",num);
    Pm_SetFilter(midi, PM_FILT_ACTIVE | PM_FILT_CLOCK);
    /* empty the buffer after setting filter, just in case anything
       got through */
    while (Pm_Poll(midi)) {
        Pm_Read(midi, buffer, 1);
    }
    i = 0;
    while (i < num) {
        status = Pm_Poll(midi);
        if (status == TRUE) {
            length = Pm_Read(midi,buffer,1);
            if (length > 0) {
                Pm_Write(midiOut, buffer, 1);
                printf("Got message %d: time %ld, %2lx %2lx %2lx\n",
                       i,
                       (long) buffer[0].timestamp,
                       (long) Pm_MessageStatus(buffer[0].message),
                       (long) Pm_MessageData1(buffer[0].message),
                       (long) Pm_MessageData2(buffer[0].message));
                i++;
            } else {
                assert(0);
            }
        }
    }

    /* close midi devices */
    Pm_Close(midi);
    Pm_Close(midiOut);
    Pm_Terminate(); 
}


/* midinote_stream exercises windows winmm API's stream mode */
/*    The winmm stream mode is used for latency>0, and sends
   timestamped messages. The timestamps are relative (delta) 
   times, whereas PortMidi times are absolute. Since peculiar
   things happen when messages are not always sent in advance,
   this function allows us to exercise the system and test it.
 */
//warning: the use of this timestamped stream of midi messages
//         provoques sending CC 64 value 0 twice to all channels 
//         tested by spi.
void midinote_stream(int latency, int deviceid, int close_mididevice_onexit, int channelid, int notenumber, int duration, int velocity) 
{
    PmStream* midi;
	char line[80];
    PmEvent buffer[3];

    /* It is recommended to start timer before PortMidi */
    TIME_START;

	/* open output device */
    Pm_OpenOutput(&midi, 
                  deviceid, 
                  DRIVER_INFO,
                  OUTPUT_BUFFER_SIZE, 
                  TIME_PROC,
                  TIME_INFO, 
                  latency);
#ifdef DEBUG
    printf("Midi Output opened with %ld ms latency.\n", (long) latency);
#endif //DEBUG

    /* if we were writing midi for immediate output, we could always use
       timestamps of zero, but since we may be writing with latency, we
       will explicitly set the timestamp to "now" by getting the time.
       The source of timestamps should always correspond to the TIME_PROC
       and TIME_INFO parameters used in Pm_OpenOutput(). */
    buffer[0].timestamp = TIME_PROC(TIME_INFO); //now in ms
    buffer[0].message = Pm_Message((unsigned char)(144+channelid), (unsigned char)notenumber, (unsigned char)velocity); //for note on, status=144+channelid
	//buffer[0].message = Pm_Message(0x90, 30, 100); //for note on, status=144+channelid
	if(duration !=-1 && duration!=0)
	{
		buffer[1].timestamp = buffer[0].timestamp + duration; //now + duration in ms
		buffer[1].message = Pm_Message((unsigned char)(144+channelid), (unsigned char)(notenumber), (unsigned char)0); //for note on, status=144+channelid
		//buffer[1].message = Pm_Message(0x90, 80, 100); //for note on, status=144+channelid
	}
	/*
	buffer[2].timestamp = buffer[0].timestamp + duration; //now
    buffer[2].message = Pm_Message((unsigned char)(144+channelid), (unsigned char)(notenumber-30), (unsigned char)velocity); //for note on, status=144+channelid
	//buffer[1].message = Pm_Message(0x90, 80, 100); //for note on, status=144+channelid

	buffer[3].timestamp = buffer[0].timestamp + 2* duration;
	//buffer[1].message = Pm_Message((unsigned char)(128+channelid), (unsigned char)notenumber, (unsigned char)0); //note off, status=128+channelid
	buffer[3].message = Pm_Message((unsigned char)(144+channelid), (unsigned char)notenumber+10, (unsigned char)velocity); //note oon, status=144+channelid
	//buffer[2].message = Pm_Message(0x90, 60, 100); //note off, status=128+channelid
    //buffer[0].message = Pm_Message(144+channelid, notenumber, 0); //note off can be replace by note on with velocity 0, status=144+channelid
	*/


	if(duration !=-1 && duration!=0)
	{
		Pm_Write(midi, buffer, 2);
		Sleep(duration+latency);

		if(close_mididevice_onexit==TRUE)
		{
			/* close device (this not explicitly needed in most implementations) */
			Pm_Close(midi);
			Pm_Terminate();
			#ifdef DEBUG
				printf("done closing and terminating...\n");
			#endif //DEBUG
		}
	}
	else
	{
		Pm_Write(midi, buffer, 1);
	}

}

//the purpose of creating nostream() version was to attempt to prevent cc 64 values being sent on all channels when closing mididevice
//by relying on Pm_WriteShort to send midi message instead of Pm_Write to send midi event (a midi message with timestamp) but did not
//find a way to send midi message without timestamp other than by specifying zero latency when opening the mididevice which provoques
//timestamps to be ignored later on when sending mesage onto this device and thereby prevents cc 64 value 0 to be sent on all channels
//upon closing mididevice.
void midinote_nostream(int latency, int deviceid, int close_mididevice_onexit, int channelid, int notenumber, int duration, int velocity) 
{
    PmStream* midi;
	char line[80];
    PmEvent buffer[3];

    /* It is recommended to start timer before PortMidi */
    TIME_START;

	/* open output device */
    Pm_OpenOutput(&midi, 
                  deviceid, 
                  DRIVER_INFO,
                  OUTPUT_BUFFER_SIZE, 
                  TIME_PROC,
                  TIME_INFO, 
                  latency);
#ifdef DEBUG
    printf("Midi Output opened with %ld ms latency.\n", (long) latency);
#endif //DEBUG

    /* writing midi for immediate output, we use timestamps of zero */
    buffer[0].timestamp = -1;
    buffer[0].message = Pm_Message((unsigned char)(144+channelid), (unsigned char)notenumber, (unsigned char)velocity); //for note on, status=144+channelid
	buffer[1].timestamp = -1; 
	buffer[1].message = Pm_Message((unsigned char)(144+channelid), (unsigned char)(notenumber), (unsigned char)0); //for note off, one way of doing it is with, same status=144+channelid, same note number but with velocity 0
	//buffer[1].message = Pm_Message((unsigned char)(128+channelid), (unsigned char)(notenumber), (unsigned char)0); //for note off, another way of doing it is with, status=128+channelid, same note number and any velocity

	if(duration !=-1 && duration!=0)
	{
		Pm_WriteShort(midi, buffer[0].timestamp, buffer[0].message);
		Sleep(duration+latency);
		Pm_WriteShort(midi, buffer[1].timestamp, buffer[1].message);

		if(close_mididevice_onexit==TRUE)
		{
			/* close device (this not explicitly needed in most implementations) */
			Pm_Close(midi);
			Pm_Terminate();
			#ifdef DEBUG
				printf("done closing and terminating...\n");
			#endif //DEBUG
		}
	}
	else
	{
		Pm_WriteShort(midi, buffer[0].timestamp, buffer[0].message);
	}

}


/* if caller alloc devicename like this: char devicename[STRING_MAX]; 
	then parameters are devicename and STRING_MAX respectively        */
int get_device_id(const char* devicename, int devicename_buffersize)
{
	int deviceid=-1;
	int i;
    for (i=0; i<Pm_CountDevices(); i++) 
	{
        const PmDeviceInfo* info = Pm_GetDeviceInfo(i);
 		if(strcmp(info->name, devicename)==0) 
		{
			deviceid=i;
			break;
		}
    }
	#ifdef DEBUG
		if(deviceid==-1)
		{
			printf("get_device_id(), did not find any matching deviceid\n");
		}
	#endif //DEBUG
	return deviceid;
}


void show_usage()
{
	int i;
    printf("Usage: midinote [-h] [-l latency-in-ms] [-d <device name>] <channel id> <note number> <duration> <velocity>\n");
	printf("\n"); 
    printf("<device name> can be one of the following midi output devices:\n");
    for (i=0; i<Pm_CountDevices(); i++) 
	{
        const PmDeviceInfo* info = Pm_GetDeviceInfo(i);
        if (info->output) printf("%d: %s\n", i, info->name);
    }
    //printf("<channel id> is an integer between 1 and 16\n");
    printf("<channel id> is an integer between 0 and 15\n");
    printf("<note number> is an integer between 0 and 127\n");
    printf("<duration> greater than 0 specify the note duration in milliseconds, use -1 or 0 to prevent any note off to be sent\n");
    printf("<velocity> is an integer between 0 and 127\n");
	printf("Usage example specifying optional latency and output device, respectively 20ms and \"Out To MIDI Yoke:  1\", along with mandatory channel id, note number, duration and velocity, respectively 1, 60, 2000ms and 100\n");
	printf("midinote -l 20 \"Out To MIDI Yoke:  1\" 1 60 2000 100\n");
	printf("Usage example not specifying optional latency and output device, in order to use the defaults, along with mandatory channel id, note number, duration and velocity, respectively 7, 80, 4000ms and 50\n");
	printf("midinote 7 80 4000 50\n");
   exit(0);
}

int main(int argc, char *argv[])
{
    int i = 0, n = 0, id=0;
    char line[STRING_MAX];

	int close_mididevice_onexit = FALSE;
	int latency_valid = FALSE;
    char devicename[STRING_MAX];
	int devicename_valid = FALSE;
	//int channelid = 0;
	int channelid = 1; //default user specified channelid must be between 1 and 16
	int channelid_valid = FALSE;
	int notenumber = 0;
	int notenumber_valid = FALSE;
	int velocity = 0;
	int velocity_valid = FALSE;
	int duration = 0;
	int duration_valid = FALSE;

	int deviceid=-1;
	const PmDeviceInfo* info;

    if(argc==1) show_usage();
    for (i = 1; i < argc; i++) 
	{
        if (strcmp(argv[i], "-h") == 0) 
		{
            show_usage();
        } 
		else if (strcmp(argv[i], "-l") == 0 && (i + 1 < argc)) 
		{
			//optional parameter, latency
            i = i + 1;
            latency = atoi(argv[i]);
            latency_valid = TRUE;
        } 
		else if ( (strcmp(argv[i], "-d") == 0 && (i + 1 < argc)) ||
				  (strlen(argv[i])>2) ) 
		{
			//optional parameter, devicename
			if(strcmp(argv[i], "-d") == 0){ i=i+1;}
            strcpy(devicename, argv[i]);
            devicename_valid = TRUE;
        } 
		else if (strcmp(argv[i], "-c") == 0) 
		{
			//optional parameter, enabling option to close device every time this program is called
            i = i + 1;
            close_mididevice_onexit = TRUE;
        } 
		else if(i + 3 < argc)
		{
			//mandatory parameters
			channelid = atoi(argv[i]);
			//min(16, max(1, channelid)); //for user channelid is specified by an integer between 1 and 16
			min(15, max(0, channelid)); //for user channelid is specified by an integer between 0 and 15
			channelid_valid = TRUE;
			//channelid = channelid -1; //for the implementation channelid is specified by an integer between 0 and 15
			notenumber = atoi(argv[i+1]);	
			min(127, max(0, notenumber));
			notenumber_valid = TRUE;
			duration = atoi(argv[i+2]);	
			//max(0, duration);
			max(-1, duration);
			duration_valid = TRUE;
			velocity = atoi(argv[i+3]);	
			min(127, max(0, velocity));
			velocity_valid = TRUE;
			break;
		}
		else 
		{
            show_usage();
        }
    }

	#ifdef DEBUG
		if (sizeof(void *) == 8) 
			printf("64-bit machine.\n");
		else if (sizeof(void *) == 4) 
			printf ("32-bit machine.\n");
		printf("latency %ld\n", (long) latency);
		printf("device name %s\n", devicename);
		printf("channel id %ld\n", (long) (channelid+1));
		printf("note number %ld\n", (long) notenumber);
		printf("duration %ld\n", (long) duration);
		printf("velocity %ld\n", (long) velocity);
	#endif //DEBUG
	if(channelid_valid==TRUE &&
		notenumber_valid==TRUE &&
		duration_valid==TRUE &&
		velocity_valid==TRUE)
	{

		if(latency_valid==FALSE)
		{
			latency = 0; //ms
		}
		if(devicename_valid==FALSE)
		{
			printf("warning wrong devicename format, replacing invalid devicename by default devicename\n");
			id = Pm_GetDefaultOutputDeviceID();
			info = Pm_GetDeviceInfo(id);
			strcpy(devicename, info->name);
			printf("device name %s\n", devicename);
		}
	}
	else
	{
		printf("warning invalid arguments format, exit(1)\n");
		show_usage();
	}

	deviceid = get_device_id(devicename, STRING_MAX);
	if(deviceid==-1) 
	{
		deviceid=Pm_GetDefaultOutputDeviceID();
		printf("warning wrong devicename syntax, replacing invalid devicename by default devicename\n");
		info = Pm_GetDeviceInfo(deviceid);
		strcpy(devicename, info->name);
		printf("device name %s\n", devicename);
	}

    /* send note */
    //midinote_stream(latency, deviceid, close_mididevice_onexit, channelid, notenumber, duration, velocity);
    midinote_nostream(latency, deviceid, close_mididevice_onexit, channelid, notenumber, duration, velocity);
	//main_test_output();

    return 0;
}
