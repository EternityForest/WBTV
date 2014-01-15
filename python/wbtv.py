import serial,time,base64
class Node():
    "Class representing one node that can send and listen for messages"
    def __init__(self, port,speed):
        "Given the name of a serial port and baudrate, init the node"
        self.s = serial.Serial(port,baudrate=speed)
        self.messages = []
        self.lastEmptiedTraffic = time.time()
        self.avgTraffic =0
        self.totalTraffic=0
        def f(x,y):
            self.messages.append((x,y))

        self.parser = Parser(f)

    def poll(self):
        """
            Process all new bytes that have been recieved since the last time this was called, and return all new messages as
            a list of tuples (channel,message) in order recieved.
        """
        
        #calculate the average traffic(note that send also records its traffic)
        self.totalTraffic+= self.s.inWaiting()
        
        timesincelastemptied =time.time()-self.lastEmptiedTraffic
        if timesincelastemptied>1:
            self.avgTraffic = (self.avgTraffic*0.95)+((self.totalTraffic/timesincelastemptied)*0.05)
            self.totalTraffic =0
            self.lastEmptiedTraffic = time.time()
            
            
        x = self.s.read(self.s.inWaiting())
        for i in x:
            self.parser.parseByte(i)
        x = self.messages
        self.messages = []
        return x

    def send(self,header,message):
        """Given a topic and message(binary strings or normal strings), send a message over the bus.
           NOTE: A PC Serial port is NOT fast enough for the CSMA stuff. You may get occasional lost messages with a normal usb to
           serial converter. Use the included leonardo usb to WBTV sketch"""
           
        #Note that this just spews the data out the port and depends on the
        x = makeMessage(header,message)
        self.totalTraffic += len(x)
        self.s.write(x)

class Hash():
    #This class implements the modulo 256 variant of the fletcher checksum
    def __init__(self,sequence = []):
        """Create an object representing a has state. The state will be initialized, and if the optional
           Arfumentsequence is provided, it will be hashed."""
        self.slow = 0
        self.fast = 0
        self.update(sequence)

    def update(self,val):
        """Update the has state by hashing either 1 integer or a byte array"""
        if hasattr(val,"__iter__"):
            for i in val:
                self.slow +=i
                self.fast += self.slow
        else:
            self.slow += val
            self.fast += self.slow

    def value(self):
        """Return the has state as a byte array"""
        return bytearray([self.slow%256,self.fast%256])

class Parser():
    def __init__(self,callback):
        self.callback = callback #this callback will be called when a message has been recieved
        self.escape = False      #If the last processed char was an escape
        self.inheader = True     #If we are currently recieving header data
        self.header = bytearray(0)
        self.message = bytearray(0)
    
    def _insbuf(self,byte):
        """Based on if we are in the header or the message, but a byte in the appropriate place"""
        if self.inheader:
            self.header.append( byte)
        else:
            self.message.append(byte)
            
    def parseByte(self,byte):
        #If the last byte was an escaped escape put this byte literally in, unset the flag and return
        if self.escape:
            self.escape = False
            self._insbuf(byte)
            return

        #This part of the code handles unescaped bytes
        #If the byte was an unescaped escape,set the escape flag and return
        if byte == ord("\\"):
            self.escape = True
            return
        #If the byte is an unescaped start of text marker, set the flag that says we are in the message not the header.
        if byte == ord("~"):
            self.inheader = False;
            return;
        #If the byte is a newline, that is the end of a message
        if byte == ord("\n"):
            h = Hash()
            #Hash the message, divider, and the checksum at the end of the message
            h.update(bytearray(self.header))
            #h.update(b"~")
            h.update(bytearray(self.message[:-2]))

            #Compare our hash with the message checksum
            if self.message[-2:]==h.value():
                #Call the callback and delegate processing our new message to it.
                self.callback(bytes(self.header), bytes(self.message[:-2]))
            else:
                #Create a message that tells the callback there was an error, if it is interested.
                self.callback(b'CONV',b"CSERR")
            return

        #If the byte is a bang, that starts a new message, discarding anything we might have been processing.
        if byte == ord("!"):
            self.inheader = True
            self.message = bytearray(0)
            self.header = bytearray(0)
            return

        #If we got this far, the byte was just a byte of data to be put in the header or message depending on the state.
        self._insbuf(byte)

def makeMessage(header,message):
    h = Hash()
    try:
        header= header.encode("utf8")
    except:
         pass
    try:
        message= message.encode("utf8")
    except:
         pass

    #every message  starts with a bang    
    data = bytearray(b'!')
    
    #Add all the bytes of the header, and hash them, but prepend escapes as needed.
    for i in bytearray(header):
        if i in ["\n","~","."]:
            data .append('\\')
            data.append(i)
        else:
            data.append( i)
        h.update(i)

    #Add the separator between message and data.
    data += b'~'
    #Same as we did for the header
    for i in bytearray(message):
        if i in ["!","~","."]:
            data.append('\\' )
            data.append(i)
        else:
            data.append(i)
        h.update(i)
        
    #Now append the two checksum bytes        
    data.append(h.value()[0])
    data.append(h.value()[1])
    #And the newline which marks the end
    data += b'\n'
    return data
