// SONIC ROBO BLAST 2
//-----------------------------------------------------------------------------
// Copyright (C) 1998-2000 by DooM Legacy Team.
// Copyright (C) 1999-2016 by Sonic Team Junior.
//
// This program is free software distributed under the
// terms of the GNU General Public License, version 2.
// See the 'LICENSE' file for more details.
//-----------------------------------------------------------------------------
/// \file  tmap.s
/// \brief optimised drawing routines for span/column rendering

// structures, must match the C structures!
#include "asm_defs.inc"

// Rappel: seuls EAX, ECX, EDX peuvent ˆtre ‚cras‚s librement.
//         il faut sauver esi,edi, cd...gs

/* Attention aux comparaisons!                                              */
/*                                                                          */
/*      Intel_compare:                                                      */
/*                                                                          */
/*              cmp     A,B                     // A-B , set flags          */
/*              jg      A_greater_than_B                                    */
/*                                                                          */
/*      AT&T_compare:                                                       */
/*                                                                          */
/*              cmp     A,B                     // B-A , set flags          */
/*              jg      B_greater_than_A                                    */
/*                                                                          */
/*        (soustrait l'op‚rande source DE l'op‚rande destination,           */
/*         comme sur Motorola! )                                            */

// RAPPEL: Intel
//         SECTION:[BASE+INDEX*SCALE+DISP]
// devient SECTION:DISP(BASE,INDEX,SCALE)

//----------------------------------------------------------------------
//
// R_DrawColumn
//
//   New optimised version 10-01-1998 by D.Fabrice and P.Boris
//   TO DO: optimise it much farther... should take at most 3 cycles/pix
//          once it's fixed, add code to patch the offsets so that it
//          works in every screen width.
//
//----------------------------------------------------------------------

    .data
#ifdef LINUX
    .align 2
#else
    .align 4
#endif
C(loopcount):   .long   0
C(pixelcount):  .long   0
C(tystep):      .long   0

C(vidwidth):    .long   0       //use this one out of the inner loops
                                //so you don't need to patch everywhere...

#ifdef USEASM
#if !defined( LINUX) && !defined( __OS2__)
    .text
#endif
.globl C(ASM_PatchRowBytes)
C(ASM_PatchRowBytes):
    pushl   %ebp
    movl    %esp, %ebp      // assure l'"adressabilit‚ du stack"

    movl    ARG1, %edx         // read first arg
    movl    %edx, C(vidwidth)

    // 1 * vidwidth
    movl    %edx,p1+2
    movl    %edx,w1+2   //water
    movl    %edx,p1b+2  //sky

    movl    %edx,p5+2
      movl    %edx,sh5+2        //smokie test

    // 2 * vidwidth
    addl    ARG1,%edx

    movl    %edx,p2+2
    movl    %edx,w2+2   //water
    movl    %edx,p2b+2  //sky

    movl    %edx,p6+2
    movl    %edx,p7+2
    movl    %edx,p8+2
    movl    %edx,p9+2
      movl    %edx,sh6+2         //smokie test
      movl    %edx,sh7+2
      movl    %edx,sh8+2
      movl    %edx,sh9+2

    // 3 * vidwidth
    addl    ARG1,%edx

    movl    %edx,p3+2
    movl    %edx,w3+2   //water
    movl    %edx,p3b+2  //sky

    // 4 * vidwidth
    addl    ARG1,%edx

    movl    %edx,p4+2
    movl    %edx,w4+2   //water
    movl    %edx,p4b+2  //sky

    popl    %ebp
    ret


#ifdef LINUX
    .align 2
#else
    .align 5
#endif
.globl C(R_DrawColumn_8)
C(R_DrawColumn_8):
    pushl   %ebp                // preserve caller's stack frame pointer
    pushl   %esi                // preserve register variables
    pushl   %edi
    pushl   %ebx

//
// dest = ylookup[dc_yl] + columnofs[dc_x];
//
    movl     C(dc_yl),%ebp
    movl     %ebp,%ebx
    movl     C(ylookup)(,%ebx,4),%edi
    movl     C(dc_x),%ebx
    addl     C(columnofs)(,%ebx,4),%edi  // edi = dest

//
// pixelcount = yh - yl + 1
//
    movl     C(dc_yh),%eax
    incl     %eax
    subl     %ebp,%eax                   // pixel count
    movl     %eax,C(pixelcount)          // save for final pixel
    jle      vdone                       // nothing to scale

//
// frac = dc_texturemid - (centery-dc_yl)*fracstep;
//
    movl     C(dc_iscale),%ecx           // fracstep
    movl     C(centery),%eax
    subl     %ebp,%eax
    imul     %ecx,%eax
    movl     C(dc_texturemid),%edx
    subl     %eax,%edx
     movl     %edx,%ebx
     shrl     $16,%ebx          // frac int.
     andl     $0x0000007f,%ebx
     shll     $16,%edx          // y frac up

     movl     %ecx,%ebp
     shll     $16,%ebp          // fracstep f. up
     shrl     $16,%ecx          // fracstep i. ->cl
     andb     $0x7f,%cl

    movl     C(dc_source),%esi

//
// lets rock :) !
//
    movl    C(pixelcount),%eax
    movb    %al,%dh
    shrl    $2,%eax
    movb    %al,%ch             // quad count
    movl    C(dc_colormap),%eax
    testb   $3,%dh
    jz      v4quadloop

//
//  do un-even pixel
//
    testb   $1,%dh
    jz      2f

    movb    (%esi,%ebx),%al     // prep un-even loops
     addl    %ebp,%edx            // ypos f += ystep f
    adcb    %cl,%bl              // ypos i += ystep i
     movb    (%eax),%dl           // colormap texel
    andb    $0x7f,%bl            // mask 0-127 texture index
     movb    %dl,(%edi)           // output pixel
    addl    C(vidwidth),%edi

//
//  do two non-quad-aligned pixels
//
2:
    testb   $2,%dh
    jz      3f

    movb    (%esi,%ebx),%al      // fetch source texel
     addl    %ebp,%edx            // ypos f += ystep f
    adcb    %cl,%bl              // ypos i += ystep i
     movb    (%eax),%dl           // colormap texel
    andb    $0x7f,%bl            // mask 0-127 texture index
     movb    %dl,(%edi)           // output pixel

    movb    (%esi,%ebx),%al      // fetch source texel
     addl    %ebp,%edx            // ypos f += ystep f
    adcb    %cl,%bl              // ypos i += ystep i
     movb    (%eax),%dl           // colormap texel
    andb    $0x7f,%bl            // mask 0-127 texture index
    addl    C(vidwidth),%edi
     movb    %dl,(%edi)           // output pixel

    addl    C(vidwidth),%edi

//
//  test if there was at least 4 pixels
//
3:
    testb   $0xFF,%ch           // test quad count
    jz      vdone

//
// ebp : ystep frac. upper 24 bits
// edx : y     frac. upper 24 bits
// ebx : y     i.    lower 7 bits,  masked for index
// ecx : ch = counter, cl = y step i.
// eax : colormap aligned 256
// esi : source texture column
// edi : dest screen
//
v4quadloop:
    movb    $0x7f,%dh           // prep mask
//    .align  4
vquadloop:
    movb    (%esi,%ebx),%al     // prep loop
     addl    %ebp,%edx            // ypos f += ystep f
    adcb    %cl,%bl              // ypos i += ystep i
     movb    (%eax),%dl           // colormap texel
    movb    %dl,(%edi)           // output pixel
     andb    $0x7f,%bl            // mask 0-127 texture index

    movb    (%esi,%ebx),%al      // fetch source texel
     addl    %ebp,%edx
    adcb    %cl,%bl
     movb    (%eax),%dl
p1:    movb    %dl,0x12345678(%edi)
     andb    $0x7f,%bl

    movb    (%esi,%ebx),%al      // fetch source texel
     addl    %ebp,%edx
    adcb    %cl,%bl
     movb    (%eax),%dl
p2:    movb    %dl,2*0x12345678(%edi)
     andb    $0x7f,%bl

    movb    (%esi,%ebx),%al      // fetch source texel
     addl    %ebp,%edx
    adcb    %cl,%bl
     movb    (%eax),%dl
p3:    movb    %dl,3*0x12345678(%edi)
     andb    $0x7f,%bl

p4:    addl    $4*0x12345678,%edi

    decb   %ch
     jnz    vquadloop

vdone:
    popl    %ebx                // restore register variables
    popl    %edi
    popl    %esi
    popl    %ebp                // restore caller's stack frame pointer
    ret

//----------------------------------------------------------------------
//
// R_DrawSpan
//
// Horizontal texture mapping
//
//----------------------------------------------------------------------

    .data

ystep:          .long   0
xstep:          .long   0
C(texwidth):    .long   64      // texture width
#if !defined( LINUX) && !defined( __OS2__)
    .text
#endif
#ifdef LINUX
    .align 2
#else
    .align 4
#endif
.globl C(R_DrawSpan_8)
C(R_DrawSpan_8):
    pushl   %ebp                // preserve caller's stack frame pointer
    pushl   %esi                // preserve register variables
    pushl   %edi
    pushl   %ebx


//
// find loop count
//
    movl    C(ds_x2),%eax
    incl    %eax
    subl    C(ds_x1),%eax               // pixel count
    movl    %eax,C(pixelcount)          // save for final pixel
    js      hdone                       // nothing to scale
    shrl    $1,%eax                     // double pixel count
    movl    %eax,C(loopcount)

//
// build composite position
//
    movl    C(ds_xfrac),%ebp
    shll    $10,%ebp
    andl    $0x0ffff0000,%ebp
    movl    C(ds_yfrac),%eax
    shrl    $6,%eax
    andl    $0x0ffff,%eax
    movl    C(ds_y),%edi
    orl     %eax,%ebp

    movl    C(ds_source),%esi

//
// calculate screen dest
//

    movl    C(ylookup)(,%edi,4),%edi
    movl    C(ds_x1),%eax
    addl    C(columnofs)(,%eax,4),%edi

//
// build composite step
//
    movl    C(ds_xstep),%ebx
    shll    $10,%ebx
    andl    $0x0ffff0000,%ebx
    movl    C(ds_ystep),%eax
    shrl    $6,%eax
    andl    $0x0ffff,%eax
    orl     %eax,%ebx

    //movl        %eax,OFFSET hpatch1+2        // convice tasm to modify code...
    movl    %ebx,hpatch1+2
    //movl        %eax,OFFSET hpatch2+2        // convice tasm to modify code...
    movl    %ebx,hpatch2+2
    movl    %esi,hpatch3+2
    movl    %esi,hpatch4+2
// %eax      aligned colormap
// %ebx      aligned colormap
// %ecx,%edx  scratch
// %esi      virtual source
// %edi      moving destination pointer
// %ebp      frac
    movl    C(ds_colormap),%eax
//    shld    $22,%ebp,%ecx           // begin calculating third pixel (y units)
//    shld    $6,%ebp,%ecx            // begin calculating third pixel (x units)
     movl    %ebp,%ecx
    addl    %ebx,%ebp               // advance frac pointer
     shrw    $10,%cx
     roll    $6,%ecx
    andl    $4095,%ecx              // finish calculation for third pixel
//    shld    $22,%ebp,%edx           // begin calculating fourth pixel (y units)
//    shld    $6,%ebp,%edx            // begin calculating fourth pixel (x units)
     movl    %ebp,%edx
     shrw    $10,%dx
     roll    $6,%edx
    addl    %ebx,%ebp               // advance frac pointer
    andl    $4095,%edx              // finish calculation for fourth pixel
    movl    %eax,%ebx
    movb    (%esi,%ecx),%al         // get first pixel
    movb    (%esi,%edx),%bl         // get second pixel
    testl   $0x0fffffffe,C(pixelcount)
    movb    (%eax),%dl             // color translate first pixel

//    jnz hdoubleloop             // at least two pixels to map
//    jmp hchecklast

//    movw $0xf0f0,%dx //see visplanes start

    jz      hchecklast
    movb    (%ebx),%dh              // color translate second pixel
    movl    C(loopcount),%esi
//    .align  4
hdoubleloop:
//    shld    $22,%ebp,%ecx        // begin calculating third pixel (y units)
//    shld    $6,%ebp,%ecx         // begin calculating third pixel (x units)
    movl    %ebp,%ecx
    shrw    $10,%cx
    roll    $6,%ecx
hpatch1:
    addl    $0x012345678,%ebp    // advance frac pointer
    movw    %dx,(%edi)           // write first pixel
    andl    $4095,%ecx           // finish calculation for third pixel
//    shld    $22,%ebp,%edx        // begin calculating fourth pixel (y units)
//    shld    $6,%ebp,%edx         // begin calculating fourth pixel (x units)
    movl    %ebp,%edx
    shrw    $10,%dx
    roll    $6,%edx
hpatch3:
    movb    0x012345678(%ecx),%al      // get third pixel
//    movb    %bl,1(%edi)          // write second pixel
    andl    $4095,%edx           // finish calculation for fourth pixel
hpatch2:
    addl    $0x012345678,%ebp    // advance frac pointer
hpatch4:
    movb    0x012345678(%edx),%bl      // get fourth pixel
    movb    (%eax),%dl           // color translate third pixel
    addl    $2,%edi              // advance to third pixel destination
    decl    %esi                 // done with loop?
    movb    (%ebx),%dh           // color translate fourth pixel
    jnz hdoubleloop

// check for final pixel
hchecklast:
    testl   $1,C(pixelcount)
    jz      hdone
    movb    %dl,(%edi)           // write final pixel

hdone:
    popl    %ebx                 // restore register variables
    popl    %edi
    popl    %esi
    popl    %ebp                 // restore caller's stack frame pointer
    ret


//.endif


//----------------------------------------------------------------------
// R_DrawTranslucentColumn_8
//
// Vertical column texture drawer, with transparency.
//----------------------------------------------------------------------

#ifdef LINUX
    .align 2
#else
    .align 5
#endif

.globl C(R_DrawTranslucentColumn_8)
C(R_DrawTranslucentColumn_8):
    pushl   %ebp                // preserve caller's stack frame pointer
    pushl   %esi                // preserve register variables
    pushl   %edi
    pushl   %ebx

//
// dest = ylookup[dc_yl] + columnofs[dc_x];
//
    movl     C(dc_yl),%ebp
    movl     %ebp,%ebx
    movl     C(ylookup)(,%ebx,4),%edi
    movl     C(dc_x),%ebx
    addl     C(columnofs)(,%ebx,4),%edi  // edi = dest

//
// pixelcount = yh - yl + 1
//
    movl     C(dc_yh),%eax
    incl     %eax
    subl     %ebp,%eax                   // pixel count
    movl     %eax,C(pixelcount)          // save for final pixel
    jle      vtdone                       // nothing to scale

//
// frac = dc_texturemid - (centery-dc_yl)*fracstep;
//
    movl     C(dc_iscale),%ecx           // fracstep
    movl     C(centery),%eax
    subl     %ebp,%eax
    imul     %ecx,%eax
    movl     C(dc_texturemid),%edx
    subl     %eax,%edx
    movl     %edx,%ebx

    shrl     $16,%ebx          // frac int.
    andl     $0x0000007f,%ebx
    shll     $16,%edx          // y frac up

    movl     %ecx,%ebp
    shll     $16,%ebp          // fracstep f. up
    shrl     $16,%ecx          // fracstep i. ->cl
    andb     $0x7f,%cl
    pushw    %cx
    movl     %edx,%ecx
    popw     %cx
    movl     C(dc_colormap),%edx
    movl     C(dc_source),%esi

//
// lets rock :) !
//
    movl    C(pixelcount),%eax
    shrl    $2,%eax
    testb   $0x03,C(pixelcount)
    movb    %al,%ch             // quad count
    movl    C(dc_transmap),%eax
    jz      vt4quadloop
//
//  do un-even pixel
//
    testb   $1,C(pixelcount)
    jz      2f

    movb    (%esi,%ebx),%ah      // fetch texel : colormap number
     addl    %ebp,%ecx
    adcb    %cl,%bl
     movb    (%edi),%al           // fetch dest  : index into colormap
    andb    $0x7f,%bl
     movb    (%eax),%dl
    movb    (%edx), %dl          // use colormap now !
    movb    %dl,(%edi)
     addl    C(vidwidth),%edi
//
//  do two non-quad-aligned pixels
//
2:
    testb   $2,C(pixelcount)
    jz      3f

    movb    (%esi,%ebx),%ah      // fetch texel : colormap number
     addl    %ebp,%ecx
    adcb    %cl,%bl
     movb    (%edi),%al           // fetch dest  : index into colormap
    andb    $0x7f,%bl
     movb    (%eax),%dl
    movb    (%edx), %dl          // use colormap now !
    movb    %dl,(%edi)
     addl    C(vidwidth),%edi

    movb    (%esi,%ebx),%ah      // fetch texel : colormap number
     addl    %ebp,%ecx
    adcb    %cl,%bl
     movb    (%edi),%al           // fetch dest  : index into colormap
    andb    $0x7f,%bl
     movb    (%eax),%dl
    movb    (%edx), %dl          // use colormap now !
    movb    %dl,(%edi)
     addl    C(vidwidth),%edi

//
//  test if there was at least 4 pixels
//
3:
    testb   $0xFF,%ch           // test quad count
    jz      vtdone

//
// tystep : ystep frac. upper 24 bits
// edx : upper 24 bit : colomap
//  dl : tmp pixel to write
// ebx : y     i.    lower 7 bits,  masked for index
// ecx : y     frac. upper 16 bits
// ecx : ch = counter, cl = y step i.
// eax : transmap aligned 65535 (upper 16 bit)
//  ah : background pixel (from the screen buffer)
//  al : foreground pixel (from the texture)
// esi : source texture column
// ebp,edi : dest screen
//
vt4quadloop:
    movb    (%esi,%ebx),%ah      // fetch texel : colormap number
p5: movb    0x12345678(%edi),%al           // fetch dest  : index into colormap

    movl    %ebp,C(tystep)
    movl    %edi,%ebp
    subl    C(vidwidth),%edi
    jmp inloop
//    .align  4
vtquadloop:
    addl    C(tystep),%ecx
    adcb    %cl,%bl
p6: addl    $2*0x12345678,%ebp
    andb    $0x7f,%bl
    movb    (%eax),%dl
    movb    (%esi,%ebx),%ah      // fetch texel : colormap number
    movb    (%edx), %dl          // use colormap now !
    movb    %dl,(%edi)
    movb    (%ebp),%al           // fetch dest  : index into colormap
inloop:
    addl    C(tystep),%ecx
    adcb    %cl,%bl
p7: addl    $2*0x12345678,%edi
    andb    $0x7f,%bl
    movb    (%eax),%dl
    movb    (%esi,%ebx),%ah      // fetch texel : colormap number
    movb    (%edx), %dl          // use colormap now !
    movb    %dl,(%ebp)
    movb    (%edi),%al           // fetch dest  : index into colormap

    addl    C(tystep),%ecx
    adcb    %cl,%bl
p8: addl    $2*0x12345678,%ebp
    andb    $0x7f,%bl
    movb    (%eax),%dl
    movb    (%esi,%ebx),%ah      // fetch texel : colormap number
    movb    (%edx), %dl          // use colormap now !
    movb    %dl,(%edi)
    movb    (%ebp),%al           // fetch dest  : index into colormap

    addl    C(tystep),%ecx
    adcb    %cl,%bl
p9: addl    $2*0x12345678,%edi
    andb    $0x7f,%bl
    movb    (%eax),%dl
    movb    (%esi,%ebx),%ah      // fetch texel : colormap number
    movb    (%edx), %dl          // use colormap now !
    movb    %dl,(%ebp)
    movb    (%edi),%al           // fetch dest  : index into colormap

    decb   %ch
     jnz    vtquadloop

vtdone:
    popl    %ebx                // restore register variables
    popl    %edi
    popl    %esi
    popl    %ebp                // restore caller's stack frame pointer
    ret

#endif // ifdef USEASM
