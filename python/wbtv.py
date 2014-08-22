import serial,time,base64,math,struct
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
    
    def setLights(self, start,data):
        d = struct.pack("<BHB",0,start,len(data))
        self.send(b"STAGE",d+bytearray(data))

    def fadeLights(self, start,data,time):
        time = int(time*48)
        d = struct.pack("<BHBB",1,start,len(data),time)
        self.send(b"STAGE",d+bytearray(data))
    
     
    def sendTime(self, accuracy):
            x = accuracy/ 255.0
            x = int(math.log(x,2)+0.51)
            y = accuracy/2**x
            self.s.flush()
            #Add half a millisecond to compensate for the average USB response delay
            #Add 50 microseconds to compensate for other delays that probably exist.
            t=time.time() + 0.00055
            self.send(b"TIME", struct.pack("<qLbB" , int(t), int((t%1)*(2**32)) , int(x), int(y)  ))
            self.s.flush()
    
    #Send random data on the rand channel.
    def sendRand(self,num=16):
        self.send('RAND',os.urandom(num))
    
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
        self.s.flush()

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
                self.slow =(self.slow+i)%256
                self.fast =(self.fast+ self.slow)%256
        else:
            self.slow =(self.slow+val)%256
            self.fast =(self.fast+ self.slow)%256

    def value(self):
        """Return the has state as a byte array"""
        return bytearray([self.fast,self.slow])

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
        if isinstance(byte,int):
            pass
        else:
            byte = ord(byte)

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
            #Hash the header,message, and divider, 
            h.update(self.header)
            h.update(b"~")
            h.update(bytearray(self.message[:-2]))

            #Compare our hash with the message checksum
            if self.message[-2:]==h.value():
                #Call the callback and delegate processing our new message to it.
                self.callback(bytearray(self.header), bytearray(self.message[:-2]))
            else:
                #Create a message that tells the callback there was an error, if it is interested.
                self.callback(b'CONV',b"CSERR"+self.header)
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
    header = bytearray(header)
    message = bytearray(message)

    #every message  starts with a bang    
    data = bytearray(b'!')
    
    #Add all the bytes of the header, and hash them, but prepend escapes as needed.
    for i in header:
        if i in [ord("\n"),ord("~"),ord("\\")]:
            data.append(ord('\\'))
            data.append(i)
        else:
            data.append( i)
        h.update(i)

    #Add the separator between message and data.
    data.append( ord('~'))
    h.update(ord('~'))
    
    #Same as we did for the header
    for i in message:
        if i in [ord("!"),ord("~"),ord("\\")]:
            data.append(ord('\\'))
            data.append(i)
        else:
            data.append(i)
        h.update(i)
        
    #Now append the two checksum bytes 
    for i in h.value():
        if i in [ord("!"),ord("~"),ord("\\")]:
            data.append(ord('\\'))
            data.append(i)
    #And the newline which marks the end
    data.append(ord('\n'))
    return data




#def internalRecieve(x,y):
#    if x == "SERV":
#        owningdev = y[0:16]
#        service = y[17:32]
#        clen = y[33]
#        channel = y[34:34+clen]
#        modifiers = y[34+clen:]
#        
#        if service in self.supportedServices:
#            self.supportedServices[service](owningdev,channel,modifiers)
#
#class BaseService(object):
#    pass
#
#class DummyStruct(object):
#    def pack(self,data):
#        return None
#    
#
#class DAQService(BaseService):
#    def __init__(self, owningdev,channel,modifiers):
#        fields= modifiers.split("\n")
#        
#        self.fields= []
#        
#        name_to_struct ={
#            "int8":"c",
#            "uint8":"B",
#            "int16":"h",
#            "uint16":"H",
#            "int32":"i",
#            "uint32":"I"
#        }
#        
#        
#        for i in fields:
#            x = i.split(" ")
#            ftype= x[0]
#            fname= x[1]
#            
#            if "as" in x:
#                funit = x[x.index['as']+1]
#            else:
#                funit = ''
#                
#            if ftype in name_to_struct:
#                fstruct = struct.Struct(name_to_strut[ftype])
#            else:
#                raise RuntimeError("Can't decoode service, bad struct field")
#            self.fields.append((fname,ftype,funit,fstruct))
#    
#    def onMessage(self,message):
#        signals ={}
#        while message:
#            signal = message[0]
#            signlen = self.fields[signal][4].size
#            signal = message[]
#            signals = messag
#    
#        
#    
#    
#    
#
#            
