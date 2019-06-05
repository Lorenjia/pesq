/* GstPEAQ
 * Copyright (C) 2013, 2014, 2015 Martin Holters <martin.holters@hsu-hh.de>
 *
 * movaccum.h: Model out variable (MOV) accumulation.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#ifndef __MOVACCUM_H__
#define __MOVACCUM_H__ 1

#include <glib-object.h>

#define PEAQ_TYPE_MOVACCUM (peaq_movaccum_get_type ())
#define PEAQ_MOVACCUM(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST (obj, PEAQ_TYPE_MOVACCUM, PeaqMovAccum))
#define PEAQ_MOVACCUM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST (klass, PEAQ_TYPE_MOVACCUM, PeaqMovAccumClass))
#define PEAQ_IS_MOVACCUM(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE (obj, PEAQ_TYPE_MOVACCUM))
#define PEAQ_IS_MOVACCUM_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE (klass, PEAQ_TYPE_MOVACCUM))
#define PEAQ_MOVACCUM_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS (obj, PEAQ_TYPE_MOVACCUM, PeaqMovAccumClass))

typedef struct _PeaqMovAccumClass PeaqMovAccumClass;
typedef struct _PeaqMovAccum PeaqMovAccum;

/**
 * PeaqMovAccumMode:
 * @MODE_AVG: Linear averaging as decribed in section 5.2.1 of <xref
 * linkend="BS1387" />, used for Segmental NMR, Error Harmonic Structure,
 * Average Linear Distortion, Bandwidth, Average Moduation Difference, and
 * Relative Distorted Frames model output variables:
 * <informalequation><math xmlns="http://www.w3.org/1998/Math/MathML">
 *   <msub><mi>X</mi><mi>c</mi></msub>
 *   <mo>=</mo>
 *   <mfrac>
 *     <mrow>
 *       <munderover>
 *         <mo>&sum;</mo>
 *         <mrow><mi>i</mi><mo>=</mo><mn>1</mn></mrow>
 *         <mi>N</mi>
 *       </munderover>
 *       <msub><mi>w</mi><mi>i</mi></msub>
 *       <mo>&InvisibleTimes;</mo>
 *       <msub><mi>x</mi><mi>i</mi></msub>
 *     </mrow>
 *     <mrow>
 *       <munderover>
 *         <mo>&sum;</mo>
 *         <mrow><mi>i</mi><mo>=</mo><mn>1</mn></mrow>
 *         <mi>N</mi>
 *       </munderover>
 *       <msub><mi>w</mi><mi>i</mi></msub>
 *     </mrow>
 *   </mfrac>
 * </math></informalequation>
 * @MODE_AVG_LOG: A variant of linear averaging which takes a logarithm in the
 * end as needed for the Total NMR model output variable, see section 4.5.1 of
 * <xref linkend="BS1387" />:
 * <informalequation><math xmlns="http://www.w3.org/1998/Math/MathML">
 *   <mrow>
 *     <msub><mi>X</mi><mi>c</mi></msub>
 *     <mo>=</mo>
 *     <mn>10</mn>
 *     <mo>&InvisibleTimes;</mo>
 *     <msub><mi>log</mi><mn>10</mn></msub>
 *     <mo>&ApplyFunction;</mo>
 *     <mfenced open="(" close=")">
 *       <mfrac>
 *         <mrow>
 *           <munderover>
 *             <mo>&sum;</mo>
 *             <mrow><mi>i</mi><mo>=</mo><mn>1</mn></mrow>
 *             <mi>N</mi>
 *           </munderover>
 *           <msub><mi>w</mi><mi>i</mi></msub>
 *           <mo>&InvisibleTimes;</mo>
 *           <msub><mi>x</mi><mi>i</mi></msub>
 *         </mrow>
 *         <mrow>
 *           <munderover>
 *             <mo>&sum;</mo>
 *             <mrow><mi>i</mi><mo>=</mo><mn>1</mn></mrow>
 *             <mi>N</mi>
 *           </munderover>
 *           <msub><mi>w</mi><mi>i</mi></msub>
 *         </mrow>
 *       </mfrac>
 *     </mfenced>
 *   </mrow>
 * </math></informalequation>
 * @MODE_RMS: Root-mean-square averaging as described in section 5.2.2 of <xref
 * linkend="BS1387" />, used for Modulation Difference and Noise Loudness model
 * output variables:
 * <informalequation><math xmlns="http://www.w3.org/1998/Math/MathML">
 *   <msub><mi>X</mi><mi>c</mi></msub>
 *   <mo>=</mo>
 *   <msqrt><mfrac>
 *     <mrow>
 *       <munderover>
 *         <mo>&sum;</mo>
 *         <mrow><mi>i</mi><mo>=</mo><mn>1</mn></mrow>
 *         <mi>N</mi>
 *       </munderover>
 *       <msup><msub><mi>w</mi><mi>i</mi></msub><mn>2</mn></msup>
 *       <mo>&InvisibleTimes;</mo>
 *       <msup><msub><mi>x</mi><mi>i</mi></msub><mn>2</mn></msup>
 *     </mrow>
 *     <mrow>
 *       <munderover>
 *         <mo>&sum;</mo>
 *         <mrow><mi>i</mi><mo>=</mo><mn>1</mn></mrow>
 *         <mi>N</mi>
 *       </munderover>
 *       <msup><msub><mi>w</mi><mi>i</mi></msub><mn>2</mn></msup>
 *     </mrow>
 *   </mfrac></msqrt>
 * </math></informalequation>
 * Note that the factor <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msqrt><mi>Z</mi></msqrt>
 * </math></inlineequation> 
 * introduced in <xref linkend="BS1387" /> for the weighted case only is not
 * included here but has be included in the calculation of <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>x</mi><mi>i</mi></msub>
 * </math></inlineequation> 
 * or when using the output of the accumulator for further calculations.
 * @MODE_RMS_ASYM: A variant of root-mean-square averaging used for the
 * Asymmetric Noise Loudness model output variable, see section 4.3.3 of <xref
 * linkend="BS1387" />:
 * <informalequation><math xmlns="http://www.w3.org/1998/Math/MathML">
 *   <mrow>
 *     <msub><mi>X</mi><mi>c</mi></msub>
 *     <mo>=</mo>
 *     <msqrt>
 *       <mrow>
 *         <mfrac><mn>1</mn><mi>N</mi></mfrac>
 *         <mo>&InvisibleTimes;</mo>
 *         <munderover>
 *           <mo>&sum;</mo>
 *           <mrow><mi>i</mi><mo>=</mo><mn>1</mn></mrow>
 *           <mi>N</mi>
 *         </munderover>
 *         <msup><msub><mi>x</mi><mi>i</mi></msub><mn>2</mn></msup>
 *       </mrow>
 *     </msqrt>
 *     <mo>+</mo>
 *     <mfrac><mn>1</mn><mn>2</mn></mfrac>
 *     <mo>&InvisibleTimes;</mo>
 *     <msqrt>
 *       <mrow>
 *         <mfrac><mn>1</mn><mi>N</mi></mfrac>
 *         <mo>&InvisibleTimes;</mo>
 *         <munderover>
 *           <mo>&sum;</mo>
 *           <mrow><mi>i</mi><mo>=</mo><mn>1</mn></mrow>
 *           <mi>N</mi>
 *         </munderover>
 *         <msup><msub><mi>w</mi><mi>i</mi></msub><mn>2</mn></msup>
 *       </mrow>
 *     </msqrt>
 *   </mrow>
 * </math></informalequation>
 * @MODE_AVG_WINDOW: Windowed averaging as described in section 5.2.3 of <xref
 * linkend="BS1387" />, used for Modulation Difference model output variable:
 * <informalequation><math xmlns="http://www.w3.org/1998/Math/MathML">
 *   <msub><mi>X</mi><mi>c</mi></msub>
 *   <mo>=</mo>
 *   <msqrt>
 *     <mrow>
 *       <mfrac><mn>1</mn><mi>N</mi></mfrac>
 *       <mo>&InvisibleTimes;</mo>
 *       <munderover>
 *         <mo>&sum;</mo>
 *         <mrow><mi>i</mi><mo>=</mo><mn>1</mn></mrow>
 *         <mi>N</mi>
 *       </munderover>
 *       <msup>
 *         <mfenced open="(" close=")">
 *           <mrow>
 *             <mfrac><mn>1</mn><mn>4</mn></mfrac>
 *             <mo>&InvisibleTimes;</mo>
 *             <munderover>
 *               <mo>&sum;</mo>
 *               <mrow><mi>j</mi><mo>=</mo><mn>i</mn><mo>-</mo><mn>3</mn></mrow>
 *               <mi>i</mi>
 *             </munderover>
 *             <msqrt><msub><mi>x</mi><mi>j</mi></msub></msqrt>
 *           </mrow>
 *         </mfenced>
 *         <mn>4</mn>
 *       </msup>
 *     </mrow>
 *   </msqrt>
 * </math></informalequation>
 * Note that no model output variable obtained from the filter bank ear model
 * uses windowed averaging, hence the longer averaging window defined in <xref
 * linkend="BS1387" /> is not supported.
 * @MODE_FILTERED_MAX: Filtered maximum as used by the Maximum Filtered
 * Probability of Detection model output variable, see section 4.7.1 in <xref
 * linkend="BS1387" />:
 * <informalequation><math xmlns="http://www.w3.org/1998/Math/MathML">
 *   <mrow>
 *     <msub><mi>X</mi><mi>c</mi></msub>
 *     <mo>=</mo>
 *     <mi>max</mi>
 *     <mo>&ApplyFunction;</mo>
 *     <mfenced open="{" close="}">
 *       <msub><mi>y</mi><mi>i</mi></msub>
 *     </mfenced>
 *     <mo> where </mo>
 *     <msub><mi>y</mi><mi>i</mi></msub>
 *     <mo>=</mo>
 *     <mn>0.9</mn>
 *     <mo>&InvisibleTimes;</mo>
 *     <msub><mi>y</mi><mrow><mi>i</mi><mo>-</mo><mn>1</mn></mrow></msub>
 *     <mo>+</mo>
 *     <mn>0.1</mn>
 *     <mo>&InvisibleTimes;</mo>
 *     <msub><mi>x</mi><mi>i</mi></msub>
 *   </mrow>
 * </math></informalequation>
 * @MODE_ADB: Special accumulation mode for the Average Distorted Block model
 * output variable, see section 4.7.2 in <xref linkend="BS1387" /> and note
 * that <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>w</mi><mi>i</mi></msub></math></inlineequation> should always be
 * set to one:
 * <informalequation><math xmlns="http://www.w3.org/1998/Math/MathML">
 *   <mrow>
 *     <msub><mi>X</mi><mi>c</mi></msub>
 *     <mo>=</mo>
 *     <mfenced open="{" close="">
 *       <mtable>
 *         <mtr>
 *           <mtd><mn>0</mn></mtd>
 *           <mtd columnalign="left">
 *             <mo>for </mo>
 *             <munderover>
 *               <mo>&sum;</mo>
 *               <mrow><mi>i</mi><mo>=</mo><mn>1</mn></mrow>
 *               <mi>N</mi>
 *             </munderover>
 *             <msub><mi>w</mi><mi>i</mi></msub>
 *             <mo>=</mo><mn>0</mn>
 *           </mtd>
 *         </mtr>
 *         <mtr>
 *           <mtd><mn>-0.5</mn></mtd>
 *           <mtd columnalign="left">
 *             <mo>for </mo>
 *             <munderover>
 *               <mo>&sum;</mo>
 *               <mrow><mi>i</mi><mo>=</mo><mn>1</mn></mrow>
 *               <mi>N</mi>
 *             </munderover>
 *             <msub><mi>w</mi><mi>i</mi></msub>
 *             <mo>&InvisibleTimes;</mo>
 *             <msub><mi>x</mi><mi>i</mi></msub>
 *             <mo>=</mo><mn>0</mn>
 *             <mtext>,</mtext><mspace width="1ex" />
 *             <munderover>
 *               <mo>&sum;</mo>
 *               <mrow><mi>i</mi><mo>=</mo><mn>1</mn></mrow>
 *               <mi>N</mi>
 *             </munderover>
 *             <msub><mi>w</mi><mi>i</mi></msub>
 *             <mo>&ne;</mo><mn>0</mn>
 *           </mtd>
 *         </mtr>
 *         <mtr>
 *           <mtd>
 *             <msub><mi>log</mi><mn>10</mn></msub>
 *             <mo>&ApplyFunction;</mo>
 *             <mfenced open="(" close=")">
 *               <mfrac>
 *                 <mrow>
 *                   <munderover>
 *                     <mo>&sum;</mo>
 *                     <mrow><mi>i</mi><mo>=</mo><mn>1</mn></mrow>
 *                     <mi>N</mi>
 *                   </munderover>
 *                   <msub><mi>w</mi><mi>i</mi></msub>
 *                   <mo>&InvisibleTimes;</mo>
 *                   <msub><mi>x</mi><mi>i</mi></msub>
 *                 </mrow>
 *                 <mrow>
 *                   <munderover>
 *                     <mo>&sum;</mo>
 *                     <mrow><mi>i</mi><mo>=</mo><mn>1</mn></mrow>
 *                     <mi>N</mi>
 *                   </munderover>
 *                   <msub><mi>w</mi><mi>i</mi></msub>
 *                 </mrow>
 *               </mfrac>
 *             </mfenced>
 *           </mtd>
 *           <mtd columnalign="left"><mo>else</mo></mtd>
 *         </mtr>
 *       </mtable>
 *     </mfenced>
 *   </mrow>
 * </math></informalequation>
 *
 * Accumulation mode of the model output variable. For all channels <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><mi>c</mi>
 * </math></inlineequation>, the accumulation over time steps <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><mi>i</mi>
 * </math></inlineequation> is performed independently according to the
 * formulas below, where <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>x</mi><mi>i</mi></msub></math></inlineequation> and <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>w</mi><mi>i</mi></msub></math></inlineequation> denote the inputs
 * to peaq_movaccum_accumulate(). The resulting per-channel values <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>X</mi><mi>c</mi></msub></math></inlineequation> are averaged to
 * obtain the final result as returned by peaq_movaccum_get_value().
 */
typedef enum
{
  MODE_AVG,
  MODE_AVG_LOG,
  MODE_RMS,
  MODE_RMS_ASYM,
  MODE_AVG_WINDOW,
  MODE_FILTERED_MAX,
  MODE_ADB
} PeaqMovAccumMode;

GType peaq_movaccum_get_type ();
PeaqMovAccum *peaq_movaccum_new ();
void peaq_movaccum_set_channels (PeaqMovAccum *acc, guint channels);
guint peaq_movaccum_get_channels (PeaqMovAccum const *acc);
void peaq_movaccum_set_mode (PeaqMovAccum *acc, PeaqMovAccumMode mode);
PeaqMovAccumMode peaq_movaccum_get_mode (PeaqMovAccum *acc);
void peaq_movaccum_set_tentative (PeaqMovAccum *acc, gboolean tentative);
void peaq_movaccum_accumulate (PeaqMovAccum *acc, guint c, gdouble val,
                               gdouble weight);
gdouble peaq_movaccum_get_value (PeaqMovAccum const *acc);

#endif
