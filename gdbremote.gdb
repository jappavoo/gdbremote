
# ******************************************************************************
# * Copyright (C) 2020 by Jonathan Appavoo, Boston University
# *
# * Permission is hereby granted, free of charge, to any person obtaining a copy
# * of this software and associated documentation files (the "Software"), to deal
# * in the Software without restriction, including without limitation the rights
# * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# * copies of the Software, and to permit persons to whom the Software is
# * furnished to do so, subject to the following conditions:
# *
# * The above copyright notice and this permission notice shall be included in
# * all copies or substantial portions of the Software.
# *
# * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# * THE SOFTWARE.
# *****************************************************************************/

define target hook-remote
# echo "remote"
end

define target hookpost-remote
  !stty sane
#  echo "remote done"
end

define setRealMode
  set arch i8086
end 
document setRealMode
  This command loads the protocol definition for 32 bit register layout 
  the architecture to i8086 for intel 16 bit dissasembly
end

define setProtectedMode
  set arch i386
end
document setProtectedMode
  This command loads the protocol definition for 32 bit register layout 
  when executing in 32 or 16 bit mode.   It also sets the 
  architecture to i386  for 32 bit intel disassembly.
end

define setLongMode
  set arch i386:x86-64	
end
document setLongMode
  This command loads the protocol definition for 64 bit register layout 
  when kex is executing in 32 or 16 bit mode.   It also sets the 
  architecture to i386  for 32 bit intel disassembly.
end

define start
  target remote | ./gdbRemote
end

define hook-quit
!stty sane
end
