import serial,time,base64,math,struct

#Return easier to work with service object representation
def parseService(service):
    o = object()
    o.provider = service[0:15]
    o.service = service[16:32]
    l = service[33]
    o.channel = service[34:34+l]
    o.modifiers = service[35+l:]
    return o

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
        while data:
            x = data[:48]
            data = data[48:]
            d = struct.pack("<BHB",0,start,len(x))
            self.send(b"STAGE",d+bytearray(x))
            start+= len(x)

    def fadeLights(self, start,data,time):
            while data:
                x = data[:48]
                data = data[48:]
            time = int(time*48)
            d = struct.pack("<BHBB",1,start,len(x),time)
            self.send(b"STAGE",d+bytearray(x))
            start+= len(x)
            
    def sendIOMessage(self,events):
        b = bytearray(0)
        recipient = 0
        
        for i in events:
            if len(b)>64:
                self.send(b'IO',b)
                b = bytearray(0)
                recipient = 0
                
            if i.recipient != recipient:
                b.extend(Event(0,1,0,'u16',i.recipient))
                recipient = i.recipient
                b.extend(i.toBytes())
            
            
     
    def sendTime(self, accuracy=60*60*24*365*30):
            x = accuracy/ 255.0
            x = int(math.log(x,2)+0.51)
            y = accuracy/2**x
            self.s.flush()
            #Add half a millisecond to compensate for the average USB response delay
            #Add 50 microseconds to compensate for other delays that probably exist.
            t=time.time() + 0.00055
            m = makeMessage(b"TIME", struct.pack("<qLbB" , int(t), int((t%1)*(2**32)) , int(x), int(y)  ))
            #Some devices have a better error estimate when you send the rest of the packet slightly after the bang
            self.s.write(m[0])
            time.sleep(0.002)
            self.s.write(m[1:])
            self.s.flush()

    def sendTZ(self):
        pass
    
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

    def send(self,header,message,block=True):
        """Given a topic and message(binary strings or normal strings), send a message over the bus.
           NOTE: A PC Serial port is NOT fast enough for the CSMA stuff. You may get occasional lost messages with a normal usb to
           serial converter. Use the included leonardo usb to WBTV sketch"""
        x = makeMessage(header,message)
        self.totalTraffic += len(x)
        self.s.write(x)
        if block:
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

    #every message  starts with a bang. Two bangs are used for a bit of noise resistance and sleep mode compatibility.    
    data = bytearray(b'!!')
    
    #Add all the bytes of the header, and hash them, but prepend escapes as needed.
    for i in header:
        if i in [ord("!"),ord("\n"),ord("~"),ord("\\")]:
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
        if i in [ord("\n"),ord("!"),ord("~"),ord("\\")]:
            data.append(ord('\\'))
            data.append(i)
        else:
            data.append(i)
        h.update(i)
        
    #Now append the two checksum bytes 
    for i in h.value():
        if i in [ord("\n"),ord("!"),ord("~"),ord("\\")]:
            data.append(ord('\\'))
            data.append(i)
        else:
            data.append(i)
    #And the newline which marks the end
    data.append(ord('\n'))
    return data

#def _getFormatByte(s,d):
    #x = 0
    #if s.startswith('u'):
        #x |= 1
    #if s.startswith('i')
        #x |= 2
    #if s.startswith('f')
        #x |= 3
    
    #if '16' in s:
        #x |= 4
    #if '32' in s:
        #x |= 8
    #if '64' in s:
        #x |= 12
    #return b


_formats = {
    'bin':  (0,None),
    'u8':   (1,"<B"), 
    'u16':  (1 + 4, '<H'), 
    'u32' : (1+8,'<L'),
    'u64' : (1+12,'<Q'),
    'i8' :  (2,'<b'),
    'i16' : (2+4,'<s'),
    'i32' : (2+8,'<l'),
    'i64' : (2+12,'<l'),
    'f32' : (3+8,'<f'),
    'f64' : (3+12,'<d')
}
     
modifiers ={
 'one':   (0,1),
 'femto': (1,10**-15),
 'pico':  (2,10**-12),
 'nano':  (3,10**-9),
 'micro': (4,10**-6),
 'milli': (5,10**-3),
 'kilo':  (6,10**-3),
 'mega':  (7,10**-6),
 'centi': (8,10**-2),
 '2_32nd':(9,2**-32),
 '2_16th':(10,2**-16),
 '2_8th': (11,2**-8),
}
     
_rev_fmts = {}
_rev_mdfy = {}
for i in _formats:
    _rev_fmts[_formats[i][0]]=(i,_formats[i][1])
    
for i in range(0,4):
    _rev_fmts[i*4] = ('bin',None)
    
for i in modifiers:
    _rev_mdfy[modifiers[i][0]]= (i,modifiers[i][1])
#}
    

class Event():
    def __init__(self,recipient, eventtype, port, dataformat, data, modifier = 'one'):
        self.recipient = recipient
        self.eventtype = eventtype
        self.port = port
        self.dataformat = dataformat
        self.data = data
        self.modifier = modifier
        
    def toBytes(self):
        b = bytearray()
        b.extend(struct.pack("<H",self.eventtype))
        b.extend(struct.pack("<B",self.port))
        x=0
        
        if self.data==None:
            b.append(0)
            return b
        
        if self.dataformat=='blob':
            if len(data)>2:
                b.extend(struct.pack('<B',len(self.data)))
                b.append(12)
                b.extend(self.data)
                return b
            
            elif len(self.data) == 1:
                x |= 4
            elif len(self.data) == 2:
                x |= 8
            b.append(x)
            b.extend(self.data)
            return b
                
        if self.dataformat == 'bool':
            if self.data:
                x|=64
            else:
                x|=32
            b.append(x)
            return b
        
                
        if self.dataformat in _formats:
            x,y = _formats[self.dataformat]
            b.append(x + (modifiers[self.modifier][0]*16))
            if y:
                b.extend(struct.pack(y,self.data))
            else:
                b.extend(self.data)
            return b
        
          
        
            


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
#            else:a
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


def parseIOMessage(data):
    data = bytearray(data)
    recipient = 0
    pointer = 0
    events = []
    length_bytes = 0
    while pointer<len(data):
        print("datais",data)
        dformat= data[pointer+3]
        decodedformat = _rev_fmts[dformat&15][0]
        if decodedformat == 'bin' and (dformat>>4)<10:
            decodedformat = ['null','false','true','bits','bin','utf8','uuid','u','i','f'][(dformat>>4)]

        if((dformat&3)>0):
            dlen = 2**((dformat>>2)&3);    
        else:
                dlen = ((dformat>>2)&3);
                if(dlen==3):
                        dlen = data[pointer+4]; length_bytes = 1;
                
        if dformat&15 in _rev_fmts:
            if _rev_fmts[dformat&15][1]:
                edata = struct.unpack(_rev_fmts[dformat&15][1],data[pointer+4:pointer+dlen+4]) [0]* _rev_mdfy[dformat>>4][1]
                if decodedformat == 'utf8':
                    edata = edata.decoode('utf8')
                if decodedformat == 'null':
                    edata = None
                if decodedformat == 'false':
                    edata = False
                if decodedformat == 'true':
                    edata = True
            else:
                if ((dformat>>2)&3) == 3: 
                    edata = data[pointer+4:pointer+dlen+5]
                else:
                    edata = data[pointer+4:pointer+5+((dformat>>2)&3)]
                    
        elif dformat == 16:
            edata = False
        elif dformat == 32:
            edata = True
        etype = struct.unpack("<H",data[pointer:pointer+2])[0]
        eport = data[pointer+2]
        if etype == 1:
            recipient = edata
            continue
        events.append(Event(recipient, etype, eport, decodedformat, edata))
        pointer += dlen+4+length_bytes
        length_bytes = 0
    return events

x =Event(56,90,8,'u64',100,'milli').toBytes()
print(x)
print(parseIOMessage(x)[0].port)
print(parseIOMessage(x)[0].dataformat)
print(parseIOMessage(x)[0].data)
print(parseIOMessage([0,25,79,76,4,80,111,111,112])[0].dataformat)