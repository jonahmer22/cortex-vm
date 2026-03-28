addi a0, zero, string
addi a13, zero, 5
syscall
halt

.data
    string: "this is a string\n\0"