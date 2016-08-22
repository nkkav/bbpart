/* "bbpart/utils.h" */
/*
 *     Copyright (c) 2004, 2005, 2006, 2007, 2008, 2009, 2010,
 *                   2011, 2012, 2013, 2014, 2015, 2016 Nikolaos Kavvadias
 *
 *     This software was written by Nikolaos Kavvadias, Ph.D. candidate
 *     at the Physics Department, Aristotle University of Thessaloniki,
 *     Greece (at the time).
 *
 *     This software is provided under the terms described in
 *     the "machine/copyright.h" include file.
 */

#include <string.h>

/* reverse: 
 * Reverse string s in place. 
 */
void reverse(char s[])
{
  int c, i, j;

  for (i = 0, j = strlen(s)-1; i < j; i++, j--) {
    c = s[i];    
    s[i] = s[j];
    s[j] = c;
  }
}          

/* itoa: 
 * Convert n to characters in s.
 */
void itoa(int n, char s[])
{
  int i, sign;
  
  if ((sign = n) < 0) { /* record sign */
    n = -n; /* make n positive */
  }    
  i = 0;
  do { /* generate digits in reverse order */
        s[i++] = n % 10 + '0'; /* get next digit */        
  } while ((n /= 10) > 0); /* delete it */
     
  if (sign < 0) {
    s[i++] = '-';
  }    
  s[i] = '\0';
  reverse(s);
}
