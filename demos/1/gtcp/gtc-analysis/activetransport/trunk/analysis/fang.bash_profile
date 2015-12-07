#
# .bash_profile    --  CoC template (rev. 09-2002)
#
# This file is sourced first for login shells.  For consistancy 
# between login and non-login interactive shells (e.g., shells
# during an X session), most important environment variables should
# be defined in .bashrc, which is sourced at the end of this file.
#
# Good candidates for inclusion in this file are things that define
# behavior of login shells only, such as greetings, message of the day,
# etc., which you do not want executed everytime you start a shell.
#
#
# automatically export environment variables to simplify things 
set -o allexport
set -o emacs

#
# e.g.  Display current date.  Linux systems will display last login date/time.
#
#date

#
# Read the message of the day (system message)
#
if [ -r /etc/motd ]; then
	cat /etc/motd
fi

# Do not delete this line!  Lest ye have no environment...
source ~/.bashrc


export CVSROOT=:pserver:fzheng@cvs.cc.gatech.edu:/net/cvs/chaos
export LD_LIBRARY_PATH=/net/hj1/ihpcl/x86_64-4AS/intel/cc/10.0/lib:~fzheng/work/rohan/lib:$LD_LIBRARY_PATH
export PATH=/net/hj1/ihpcl/bin:$PATH

# java
#export JAVA_HOME=$HOME/java/jdk1.6.0_21
#export CLASSPATH=$JAVA_HOME/lib/tools.jar:$JAVA_HOME/jre/lib/rt.jar:.
#export PATH=$JAVA_HOME/bin:$PATH

# java for streams
export JAVA_HOME=$HOME/ibm/opt/ibm/java-x86_64-60
export CLASSPATH=$JAVA_HOME/lib/tools.jar:$JAVA_HOME/jre/lib/rt.jar:.
export PATH=$JAVA_HOME/bin:$PATH

export PATH=$HOME/ibm/usr/bin:$PATH
export LD_LIBRARY_PATH=$HOME/ibm/usr/lib:$HOME/ibm/usr/lib64:$LD_LIBRARY_PATH

export LD_LIBRARY_PATH=/net/hj1/ihpcl/x86_64-4AS/openmpi-1.2.6/lib:/net/hj1/ihpcl/x86_64-4AS/intel/cc/10.0/lib:$LD_LIBRARY_PATH
export FASTLIBPATH=/net/hu1/fzheng/work/fastlib/fastlib2
export PATH="$FASTLIBPATH/script:$PATH"

# for simics
export PATH=/net/hu1/fzheng/streams/simics/simics-pkg-20-3.0.31-linux64/simics-3.0-install/simics-3.0.31/bin:$PATH
