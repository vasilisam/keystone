#!/usr/bin/expect -f

spawn make run

set timeout 15
#Wait for login and password prompt
expect "login:"
send "root\r"

set timeout 1
expect "Password:"
send "sifive\r"

expect "# "
send "modprobe keystone-driver\r"
expect "# "
send "cd /usr/share/keystone/examples/\r"

#Give control back to user
interact