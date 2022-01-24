/*******************************************************************************
*  Filename:        hal_defs.h
*  Revised:         $Date: 2013-04-19 15:42:53 +0200 (fr, 19 apr 2013) $
*  Revision:        $Revision: 9885 $
*
*  Description:     HAL defines.
*
*  Copyright (C) 2013 Texas Instruments Incorporated - http://www.ti.com/
*
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*    Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
*
*    Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the distribution.
*
*    Neither the name of Texas Instruments Incorporated nor the names of
*    its contributors may be used to endorse or promote products derived
*    from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
*  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
*  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
*  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
*  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
*  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
*  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
*  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
*  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*******************************************************************************/

#ifndef HAL_DEFS_H
#define HAL_DEFS_H
/*******************************************************************************
* CONSTANTS AND DEFINES
*/

#ifndef TRUE
#define TRUE 1
#else
#ifdef __IAR_SYSTEMS_ICC__
//#warning "Macro TRUE already defined"
#endif
#endif

#ifndef FALSE
#define FALSE 0
#else
#ifdef __IAR_SYSTEMS_ICC__
//#warning "Macro FALSE already defined"
#endif
#endif

#if 0
#ifndef NULL
#define NULL (void *)0
#else
#ifdef __IAR_SYSTEMS_ICC__
#warning "Macro NULL already defined"
#endif
#endif

#ifndef SUCCESS
#define SUCCESS 0
#else
#warning "Macro SUCCESS already defined"
#endif
#endif

#ifndef FAILED
#ifndef WIN32
#define FAILED 1
#endif
#else
#ifdef __IAR_SYSTEMS_ICC__
#warning "Macro FAILED already defined"
#endif
#endif

/*******************************************************************************
* HOST TO NETWORK BYTE ORDER MACROS
*/
#ifdef BIG_ENDIAN
#if defined(ewarm) || defined(__ICCARM__)
#define UINT16_HTON(x) st(utilReverseBuf((unsigned char *)&x, sizeof(unsigned short));)
#define UINT16_NTOH(x) st(utilReverseBuf((unsigned char *)&x, sizeof(unsigned short));)

#define UINT32_HTON(x) st(utilReverseBuf((unsigned char *)&x, sizeof(unsigned long));)
#define UINT32_NTOH(x) st(utilReverseBuf((unsigned char *)&x, sizeof(unsigned long));)
#else
#define UINT16_HTON(x) st(utilReverseBuf((unsigned char *)&x, sizeof(unsigned short));)
#define UINT16_NTOH(x) st(utilReverseBuf((unsigned char *)&x, sizeof(unsigned short));)

#define UINT32_HTON(x) st(utilReverseBuf((unsigned char *)&x, sizeof(unsigned long));)
#define UINT32_NTOH(x) st(utilReverseBuf((unsigned char *)&x, sizeof(unsigned long));)
#endif /* ifdef ewarm */
#else
#define UINT16_HTON(x)
#define UINT16_NTOH(x)

#define UINT32_HTON(x)
#define UINT32_NTOH(x)
#endif

/*
*  This macro is for use by other macros to form a fully valid C statement.
*  Without this, the if/else conditionals could show unexpected behavior.
*
*  For example, use...
*    #define SET_REGS()  st( ioreg1 = 0; ioreg2 = 0; )
*  instead of ...
*    #define SET_REGS()  { ioreg1 = 0; ioreg2 = 0; }
*  or
*    #define  SET_REGS()    ioreg1 = 0; ioreg2 = 0;
*  The last macro would not behave as expected in the if/else construct.
*  The second to last macro will cause a compiler error in certain uses
*  of if/else construct
*
*  It is not necessary, or recommended, to use this macro where there is
*  already a valid C statement.  For example, the following is redundant...
*    #define CALL_FUNC()   st(  func();  )
*  This should simply be...
*    #define CALL_FUNC()   func()
*
* (The while condition below evaluates false without generating a
*  constant-controlling-loop type of warning on most compilers.)
*/
#define st(x)                                                                                      \
	do {                                                                                       \
		x                                                                                  \
	} while (__LINE__ == -1)

#endif // #ifndef HAL_DEFS_H
