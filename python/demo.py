import wbtv,sys

if sys.version_info < (3,0):
    inp = raw_input
else:
    inp = input

port = inp("Enter the name of your serial port: ")
speed= inp("Enter baud: ")
acc= inp("How accurate is your clock(max error in seconds)? ")


n = wbtv.Node(port,int(speed))
n.sendTime(int(acc))
while(1):
    x = n.poll()
    if x:
        print(x)