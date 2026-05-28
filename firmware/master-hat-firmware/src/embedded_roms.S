// Placeholder ROM images (0xFF). Replace with real .incbin when binaries are available.
.section .rodata

.global rs232_pak_bin
.type rs232_pak_bin, %object
rs232_pak_bin:
.fill 8192, 1, 0xFF

.global rs232_pak_bin_len
.type rs232_pak_bin_len, %object
rs232_pak_bin_len:
.word 8192

.global disk_basic_11_bin
.type disk_basic_11_bin, %object
disk_basic_11_bin:
.fill 16384, 1, 0xFF

.global disk_basic_11_bin_len
.type disk_basic_11_bin_len, %object
disk_basic_11_bin_len:
.word 16384
