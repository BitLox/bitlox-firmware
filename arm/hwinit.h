/** \file hwinit.h
  *
  * \brief This describes hardware-specific initialisation functions.
  *
  * This file is licensed as described by the file LICENCE.
  */

#ifndef HWINIT_H_INCLUDED
#define HWINIT_H_INCLUDED
#ifdef __cplusplus
     extern "C" {
#endif

//extern void initAdc(void);
extern void initUsart(void);
extern void usartSpew(void);



#ifdef __cplusplus
     }
#endif
#endif // #ifndef HWINIT_H_INCLUDED
