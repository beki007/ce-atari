| Simple GEMDOS handler
| Miro Kropacek, 2013 & 2014
| miro.kropacek@gmail.com

	.globl	_gemdos_handler
	.globl	_gemdos_table
	.globl	_old_gemdos_handler
	.globl	_useOldGDHandler
	.globl	_pexec_postProc
    .globl  _pexec_callOrig
    .globl  _pexec_basepage
    .globl  _pexec_cmdline;
    .globl  _pexec_envstr;
    .globl  _pcustom_pexec2;
    
| ------------------------------------------------------
	.text
| ------------------------------------------------------

	.ascii	"XBRA"
	.ascii	"CEDD"
_old_gemdos_handler:
	.long	0

| GEMDOS call looks on the stack like this:
| paramN
| :
| param2
| param1
| function number
| return address (long)
| stack frame (word)	<--- (sp)

_gemdos_handler:
	tst.w	_useOldGDHandler
	bne.b	callOriginalHandler
	
	lea     2+4(sp),a0				        | a0 points to the function number now
	btst.b	#5,(sp)					        | check the S bit in the stack frame
	bne.b	gemdos_call
	move	usp,a0					        | if not called from SV, take params from the user stack

gemdos_call:
	move.w	(a0)+,d0				        | fn
	cmp.w	#0x100,d0				        | number of entries in the function table
	bhs.b	callOriginalHandler		        | >=0x100 are MiNT functions

   	cmp.w   #0x004b,d0                      | Pexec()?
	bne.b   callCustomHandlerNotPexec       | not Pexec(), just call the custom handler

    bra     callCustomHandlerForPexec


|============================================    
| This short piece of code calls the original GEMDOS handler.    

callOriginalHandler:
    clr.w   _useOldGDHandler                | ensure that is our handler still alive after the call (which may not return).
	move.l  _old_gemdos_handler(pc),-(sp)	| Fake a return
	rts		                                | to old code.

    
|============================================    
| Following code calls custom handler for any GEMDOS function other than Pexec() (they don't need any special processing).

callCustomHandlerNotPexec:                  |    
	lea     _gemdos_table,a1                | get the pointer to custrom handlers table
	add.w   d0,d0                           | fn*4 because it's a function pointer table
	add.w   d0,d0                           |
	adda.w  d0,a1
	
    tst.l   (a1)
	beq.b   callOriginalHandler             | if A1 == NULL, we don't have custom handler and use original handler
	
    movea.l (a1),a1                         | custom function pointer
	move.l  a0,-(sp)                        | param #1: stack pointer with function params
	jsr     (a1)                            | call the custom handler
    addq.l  #4,sp

    rte                                     | return from exception, d0 contains return code

    
|============================================    
callCustomHandlerForPexec:    
	cmpi.w  #0,(a0)					        | PE_LOADGO?
	beq.b   handleThisPexec 		        | this is PE_LOADGO, handle it!

	cmpi.w  #3,(a0)					        | PE_LOAD?
	beq.b   handleThisPexec 		        | this is PE_LOAD, handle it!

    bra.b   callOriginalHandler             | this is not PE_LOADGO and PE_LOAD, so call the original Pexec()
    
handleThisPexec:                            
| If we got here, it's Pexec() with PE_LOADGO or PE_LOAD.
| In the C part set the pexec_postProc to non-zero at the end of the handler 
| *AFTER* you called any GEMDOS functions, and it will call the original Pexec() with PE_GO param.

	lea     _gemdos_table,a1                | get the pointer to custrom handlers table
	add.w   d0,d0                           | fn*4 because it's a function pointer table
	add.w   d0,d0                           |
	adda.w  d0,a1
    
	tst.l   (a1)
	beq.b   callOriginalHandler             | if we don't have custom (Pexec) handler, call original handler
    
	movea.l (a1),a1
	move.l  a0,-(sp)                        | param #1: stack pointer with function params
	jsr     (a1)                            | call the custom handler
    addq.l  #4,sp

afterCustomPexecCall:
    
    tst.w   _pexec_callOrig                 | if this is set to non-zero, call original handler now - when the C code didn't handle it and we should try it with original handler.
    bne.b   callOriginalPexec

    tst.w   _pexec_basepage                 | should we call Pexec(PE_BASEPAGE)?
    bne.b   callPexecBasepage
    
	tst.w   _pexec_postProc                 | should we now do the post-process of PE_LOADGO by calling PE_GO?
	bne.b   callPexecGo                     | yes, call Pexec()
	
    rte                                     | return from exception, d0 contains return code
    
callPexecGo:    
| The original call was Pexec(PE_LOADGO), the Pexec(PE_LOAD) part was done in C code.
| Now this asm part will call Pexec(PE_GO), the basepage parameter is in d0 already.
|
| Transform from: int32_t Pexec (0, int8_t *name, int8_t *cmdline, int8_t *env);
|             to: int32_t Pexec (4, 0L, PD *basepage, 0L);
	
	clr.w   _pexec_postProc                 | clear the pexec_postProc flag

    move.w  #4, 8(sp)                       | set mode to PE_GO
    move.l  #0, 10(sp)                      | fname = 0
    move.l  d0, 14(sp)                      | cmdline = pointer to base page
    move.l  #0, 18(sp)                      | envstr
    bra     callOriginalHandler             | ...and jump to old handler
    
|------------------
callOriginalPexec:    
    clr.w   _pexec_callOrig                 | clear flag that original pexec should be called
    bra     callOriginalHandler             | ...and jump to old handler

|------------------    
callPexecBasepage:
    clr.w   _pexec_basepage                 | don't call this again until required

    move.l  _pexec_envstr, -(sp)            | envstr
    move.l  _pexec_cmdline, -(sp)           | cmdline
    move.l  #0, -(sp)                       | fname
    move.w  #5, -(sp)                       | PE_BASEPAGE
    move.w  #0x4b, -(sp)                    | Pexec()
    trap    #1
    add.l   #16, sp
   
    move.l  _pcustom_pexec2, a1             | A1 now contains pointer to custom_pexec2() function
    move.l  d0,-(sp)                        | param #1: return value from Pexec(PE_BASEPAGE)
    jsr     (a1)                            | call the custom handler
    addq.l  #4,sp
    
    bra     afterCustomPexecCall            | after finishing custom handler jump where we would continue without this 
    
|============================================     
    
	.data
_pexec_postProc:    dc.w    0       | 1 = post-process PE_LOADGO
_pexec_callOrig:    dc.w    0       | 1 = after custom handler call original Pexec (no post processing)

_pexec_basepage:    dc.w    0       | 1 = call original Pexec(PE_BASEPAGE) and call the 2nd part of custom_pexec
_pexec_cmdline:     dc.l    0       | pointer to cmd line string, used with pexec_basepage 
_pexec_envstr:      dc.l    0       | pointer to env string, used with pexec_basepage 
_pcustom_pexec2:    dc.l    0       | pointer to custom pexec2 function, which is called after basepage creation

