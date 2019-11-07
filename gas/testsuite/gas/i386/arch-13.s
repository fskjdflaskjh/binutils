# Test -march=
	.text

#SMAP feature
	clac
	stac
#ADCX ADOX 
	adcx    %edx, %ecx
	adox    %edx, %ecx
#RDSEED
	rdseed    %eax
#CLZERO
	clzero
#XSAVEC
	xsavec  (%ecx)
#XSAVES
	xsaves  (%ecx)
#CLFLUSHOPT
	clflushopt      (%ecx)
	monitorx %eax, %ecx, %edx
	monitorx %ax, %ecx, %edx
	monitorx
	mwaitx %eax, %ecx, %ebx
	mwaitx

#CLWB instruction
	clwb	(%ecx)	 # CLWB
	clwb	-123456(%esp,%esi,8)	 # CLWB

# mcommit instruction
	mcommit

# rdpid instruction
	rdpid %eax

# rdpru instruction
	rdpru

# wbnoinvd instruction
	wbnoinvd
