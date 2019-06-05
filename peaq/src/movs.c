/* GstPEAQ
 * Copyright (C) 2013, 2014, 2015 Martin Holters <martin.holters@hsu-hh.de>
 *
 * movs.h: Model Output Variables.
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

/**
 * SECTION:movs
 * @short_description: Model output variable calculation.
 * @title: MOVs
 *
 * The functions herein are used to calculate the model output variables
 * (MOVs). They have to be called once per frame and use one or more given
 * #PeaqMovAccum instances to accumulate the MOV. Note that the #PeaqMovAccum
 * instances have to be set up correctly to perform the correct type of
 * accumulation.
 */

#include "movs.h"
#include "settings.h"

#include <gst/fft/gstfftf64.h>
#include <math.h>
#include <string.h>

#define FIVE_DB_POWER_FACTOR 3.16227766016838
#define ONE_POINT_FIVE_DB_POWER_FACTOR 1.41253754462275
#define MAXLAG 256

static gdouble calc_noise_loudness (gdouble alpha, gdouble thres_fac, gdouble S0,
                                    gdouble NLmin,
                                    PeaqModulationProcessor const *ref_mod_proc,
                                    PeaqModulationProcessor const *test_mod_proc,
                                    gdouble const *ref_excitation,
                                    gdouble const *test_excitation);

/**
 * peaq_mov_modulation_difference:
 * @ref_mod_proc: Modulation processors of the reference signal (one per
 * channel).
 * @test_mod_proc: Modulation processors of the test signal (one per channel).
 * @mov_accum1: Accumulator for the AvgModDiff1B or RmsModDiffA MOVs.
 * @mov_accum2: Accumulator for the AvgModDiff2B MOV or NULL.
 * @mov_accum_win: Accumulator for the WinModDiff1B MOV or NULL.
 *
 * Calculates the modulation difference based MOVs as described in section 4.2
 * of <xref linkend="BS1387" />. Given the modulation patterns <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>Mod</mi><mi>Ref</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 * </math></inlineequation> 
 * and <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>Mod</mi><mi>Test</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 * </math></inlineequation> 
 * of reference and test signal, as obtained from @ref_mod_proc and
 * @test_mod_proc with peaq_modulationprocessor_get_modulation(), the
 * modulation difference is calculated according to
 * <informalequation><math xmlns="http://www.w3.org/1998/Math/MathML">
 *   <mi>ModDiff</mi><mo>=</mo>
 *   <mfrac><mn>100</mn><mover accent="true"><mi>Z</mi><mi>^</mi></mover></mfrac>
 *   <mo>&InvisibleTimes;</mo>
 *   <munderover>
 *     <mo>&sum;</mo>
 *     <mrow><mi>k</mi><mo>=</mo><mn>0</mn></mrow>
 *     <mrow><mi>Z</mi><mo>-</mo><mn>1</mn></mrow>
 *   </munderover>
 *   <mi>w</mi><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *   <mo>&InvisibleTimes;</mo>
 *   <mfrac>
 *     <mfenced open="|" close="|">
 *       <mrow>
 *         <msub><mi>Mod</mi><mi>Test</mi></msub>
 *         <mfenced open="[" close="]"><mi>k</mi></mfenced>
 *         <mo>-</mo>
 *         <msub><mi>Mod</mi><mi>Ref</mi></msub>
 *         <mfenced open="[" close="]"><mi>k</mi></mfenced>
 *       </mrow>
 *     </mfenced>
 *     <mrow>
 *       <mi>offset</mi>
 *       <mo>+</mo>
 *       <msub><mi>Mod</mi><mi>Ref</mi></msub>
 *       <mfenced open="[" close="]"><mi>k</mi></mfenced>
 *     </mrow>
 *   </mfrac>
 *   <mspace width="2em" />
 *   <mtext> where </mtext>
 *   <mspace width="2em" />
 *   <mi>w</mi><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *   <mo>=</mo>
 *   <mfenced open="{" close="">
 *     <mtable>
 *       <mtr>
 *         <mtd><mn>1</mn></mtd>
 *         <mtd>
 *           <mtext>if </mtext>
 *           <msub><mi>Mod</mi><mi>Test</mi></msub>
 *           <mfenced open="[" close="]"><mi>k</mi></mfenced>
 *           <mo>&ge;</mo>
 *           <msub><mi>Mod</mi><mi>Ref</mi></msub>
 *           <mfenced open="[" close="]"><mi>k</mi></mfenced>
 *         </mtd>
 *       </mtr>
 *       <mtr><mtd><mi>negWt</mi></mtd><mtd><mtext>else</mtext></mtd></mtr>
 *     </mtable>
 *   </mfenced>
 * </math></informalequation> 
 * and <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><mi>Z</mi>
 * </math></inlineequation> 
 * denotes the number of bands. The parameters <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><mi>offset</mi>
 * </math></inlineequation> 
 * and <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><mi>negWt</mi>
 * </math></inlineequation> 
 * are chosen as:
 * <table>
 *   <tbody>
 *     <tr>
 *       <td>
 *         <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML">
 *           <mi>offset</mi><mo>=</mo><mn>1</mn>
 *         </math></inlineequation> 
 *       </td>
 *       <td>
 *         <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML">
 *           <mi>negWt</mi><mo>=</mo><mn>1</mn>
 *         </math></inlineequation> 
 *       </td>
 *       <td>for @mov_accum1 and @mov_accum_win</td>
 *     </tr>
 *     <tr>
 *       <td>
 *         <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML">
 *           <mi>offset</mi><mo>=</mo><mn>0.01</mn>
 *         </math></inlineequation> 
 *       </td>
 *       <td>
 *         <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML">
 *           <mi>negWt</mi><mo>=</mo><mn>0.1</mn>
 *         </math></inlineequation> 
 *       </td>
 *       <td>for @mov_accum2.</td>
 *     </tr>
 *   </tbody>
 * </table>
 * If the accumulation mode of @mov_accum1 is #MODE_RMS, then <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><mover accent="true"><mi>Z</mi><mi>^</mi></mover><mo>=</mo><msqrt><mi>Z</mi></msqrt>
 * </math></inlineequation> 
 * to handle the special weighting with <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msqrt><mi>Z</mi></msqrt>
 * </math></inlineequation> 
 * introduced in (92) of <xref linkend="BS1387" />, otherwise <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><mover accent="true"><mi>Z</mi><mi>^</mi></mover><mo>=</mo><mi>Z</mi>
 * </math></inlineequation>.
 *
 * Accumulation of @mov_accum1 and @mov_accum2 (if provided) is weighted with
 * <informalequation><math xmlns="http://www.w3.org/1998/Math/MathML">
 *   <mi>TempWt</mi><mo>=</mo>
 *   <munderover>
 *     <mo>&sum;</mo>
 *     <mrow><mi>k</mi><mo>=</mo><mn>0</mn></mrow>
 *     <mrow><mi>Z</mi><mo>-</mo><mn>1</mn></mrow>
 *   </munderover>
 *   <mfrac>
 *     <mrow>
 *       <msub><mover accent="true"><mi>E</mi><mi>-</mi></mover><mi>Ref</mi></msub>
 *       <mfenced open="[" close="]"><mi>k</mi></mfenced>
 *     </mrow>
 *     <mrow>
 *       <msub><mover accent="true"><mi>E</mi><mi>-</mi></mover><mi>Ref</mi></msub>
 *       <mfenced open="[" close="]"><mi>k</mi></mfenced>
 *       <mo>+</mo>
 *       <mi>levWt</mi>
 *       <mo>&sdot;</mo>
 *       <msup>
 *         <mrow>
 *           <msub><mi>E</mi><mi>Thres</mi></msub>
 *           <mfenced open="[" close="]"><mi>k</mi></mfenced>
 *         </mrow>
 *         <mn>0.3</mn>
 *       </msup>
 *     </mrow>
 *   </mfrac>
 * </math></informalequation> 
 * where <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mover accent="true"><mi>E</mi><mi>-</mi></mover><mi>Ref</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 * </math></inlineequation> 
 * is the average loudness obtained form @ref_mod_proc with
 * peaq_modulationprocessor_get_average_loudness(), <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>E</mi><mi>Thres</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 * </math></inlineequation> 
 * is the internal ear noise as returned by peaq_earmodel_get_internal_noise(),
 * and <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><mi>levWt</mi><mo>=</mo><mn>1</mn>
 * </math></inlineequation> 
 * if @mov_accum2 is NULL and <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><mi>levWt</mi><mo>=</mo><mn>100</mn>
 * </math></inlineequation> 
 * otherwise.
 */
void
peaq_mov_modulation_difference (PeaqModulationProcessor* const *ref_mod_proc,
                                PeaqModulationProcessor* const *test_mod_proc,
                                PeaqMovAccum *mov_accum1,
                                PeaqMovAccum *mov_accum2,
                                PeaqMovAccum *mov_accum_win)
{
  guint c;
  PeaqEarModel *ear_model =
    peaq_modulationprocessor_get_ear_model (ref_mod_proc[0]);
  guint band_count = peaq_earmodel_get_band_count (ear_model);

  gdouble levWt = mov_accum2 ? 100. : 1.;
  for (c = 0; c < peaq_movaccum_get_channels (mov_accum1); c++) {
    guint i;
    gdouble const *modulation_ref =
      peaq_modulationprocessor_get_modulation (ref_mod_proc[c]);
    gdouble const *modulation_test =
      peaq_modulationprocessor_get_modulation (test_mod_proc[c]);
    gdouble const *average_loudness_ref =
      peaq_modulationprocessor_get_average_loudness (ref_mod_proc[c]);

    gdouble mod_diff_1b = 0.;
    gdouble mod_diff_2b = 0.;
    gdouble temp_wt = 0.;
    for (i = 0; i < band_count; i++) {
      gdouble w;
      gdouble diff = ABS (modulation_ref[i] - modulation_test[i]);
      /* (63) in [BS1387] with negWt = 1, offset = 1 */
      mod_diff_1b += diff / (1. + modulation_ref[i]);
      /* (63) in [BS1387] with negWt = 0.1, offset = 0.01 */
      w = modulation_test[i] >= modulation_ref[i] ? 1. : .1;
      mod_diff_2b += w * diff / (0.01 + modulation_ref[i]);
      /* (65) in [BS1387] with levWt = 100 if more than one accumulator is
         given, 1 otherwise */
      temp_wt += average_loudness_ref[i] /
        (average_loudness_ref[i] +
         levWt * pow (peaq_earmodel_get_internal_noise (ear_model, i), 0.3));
    }
    if (peaq_movaccum_get_mode(mov_accum1) == MODE_RMS)
      mod_diff_1b *= 100. / sqrt (band_count);
    else
      mod_diff_1b *= 100. / band_count;
    mod_diff_2b *= 100. / band_count;
    peaq_movaccum_accumulate (mov_accum1, c, mod_diff_1b, temp_wt);
    if (mov_accum2)
      peaq_movaccum_accumulate (mov_accum2, c, mod_diff_2b, temp_wt);
    if (mov_accum_win)
      peaq_movaccum_accumulate (mov_accum_win, c, mod_diff_1b, 1.);
  }
}

/**
 * peaq_mov_noise_loudness:
 * @ref_mod_proc: Modulation processors of the reference signal (one per
 * channel).
 * @test_mod_proc: Modulation processors of the test signal (one per channel).
 * @level: Level adapters (one per channel).
 * @mov_accum: Accumulator for the RmsNoiseLoudB MOV.
 *
 * Calculates the RmsNoiseLoudB model output variable as
 * <informalequation><math xmlns="http://www.w3.org/1998/Math/MathML">
 *   <mi>NL</mi><mo>=</mo>
 *   <mfrac><mn>24</mn><mi>Z</mi></mfrac>
 *   <mo>&InvisibleTimes;</mo>
 *   <munderover>
 *     <mo>&sum;</mo>
 *     <mrow><mi>k</mi><mo>=</mo><mn>0</mn></mrow>
 *     <mrow><mi>Z</mi><mo>-</mo><mn>1</mn></mrow>
 *   </munderover>
 *   <msup>
 *     <mfenced>
 *       <mfrac>
 *         <mrow>
 *           <msub><mi>E</mi><mi>Thres</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *         </mrow>
 *         <mrow>
 *           <msub><mi>s</mi><mi>test</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *         </mrow>
 *       </mfrac>
 *     </mfenced>
 *     <mn>0.23</mn>
 *   </msup>
 *   <mfenced>
 *     <mrow>
 *       <msup>
 *         <mfenced>
 *           <mrow>
 *             <mn>1</mn>
 *             <mo>+</mo>
 *             <mfrac>
 *               <mrow>
 *                 <mi>max</mi>
 *                 <mfenced>
 *                   <mrow>
 *                     <msub><mi>s</mi><mi>test</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *                     <mo>&sdot;</mo>
 *                     <msub><mi>E</mi><mrow><mi>P</mi><mo>,</mo><mi>Test</mi></mrow></msub>
 *                     <mfenced open="[" close="]"><mi>k</mi></mfenced>
 *                     <mo>-</mo>
 *                     <msub><mi>s</mi><mi>ref</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *                     <mo>&sdot;</mo>
 *                     <msub><mi>E</mi><mrow><mi>P</mi><mo>,</mo><mi>Ref</mi></mrow></msub>
 *                     <mfenced open="[" close="]"><mi>k</mi></mfenced>
 *                   </mrow>
 *                   <mn>0</mn>
 *                 </mfenced>
 *               </mrow>
 *               <mrow>
 *                 <msub><mi>E</mi><mi>Thres</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *                 <mo>+</mo>
 *                 <msub><mi>s</mi><mi>ref</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *                 <mo>&sdot;</mo>
 *                 <msub><mi>E</mi><mrow><mi>P</mi><mo>,</mo><mi>Ref</mi></mrow></msub>
 *                 <mfenced open="[" close="]"><mi>k</mi></mfenced>
 *                 <mo>&sdot;</mo>
 *                 <mi>&beta;</mi><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *               </mrow>
 *             </mfrac>
 *           </mrow>
 *         </mfenced>
 *         <mn>0.23</mn>
 *       </msup>
 *       <mo>-</mo>
 *       <mn>1</mn>
 *     </mrow>
 *   </mfenced>
 * </math></informalequation> 
 * where <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>E</mi><mi>Thres</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 * </math></inlineequation> 
 * is the internal ear noise as returned by peaq_earmodel_get_internal_noise(), <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>E</mi><mrow><mi>P</mi><mo>,</mo><mi>Ref</mi></mrow></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 * </math></inlineequation>
 * and <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>E</mi><mrow><mi>P</mi><mo>,</mo><mi>Test</mi></mrow></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 * </math></inlineequation>
 * are the spectrally adapted patterns of the reference and test signal as
 * returned by peaq_leveladapter_get_adapted_ref() and
 * peaq_leveladapter_get_adapted_test(), respectively, <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>s</mi><mi>test</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced><mo>=</mo><mn>0.15</mn><mo>&sdot;</mo><msub><mi>Mod</mi><mi>test</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced><mo>+</mo><mn>0.5</mn>
 * </math></inlineequation> 
 * and <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>s</mi><mi>ref</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced><mo>=</mo><mn>0.15</mn><mo>&sdot;</mo><msub><mi>Mod</mi><mi>ref</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced><mo>+</mo><mn>0.5</mn>
 * </math></inlineequation> 
 * are computed from the modulation <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>Mod</mi><mi>test</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 * </math></inlineequation>
 * and <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>Mod</mi><mi>ref</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 * </math></inlineequation>
 * of the test and reference signal, respectively, as obtained with
 * peaq_modulationprocessor_get_modulation() and <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><mi>&beta;</mi><mfenced open="[" close="]"><mi>k</mi></mfenced><mo>=</mo><mi>exp</mi><mfenced><mrow><mn>-1.5</mn><mo>&sdot;</mo><mfenced><mrow><msub><mi>E</mi><mrow><mi>P</mi><mo>,</mo><mi>Test</mi></mrow></msub><mfenced open="[" close="]"><mi>k</mi></mfenced><mo>-</mo><msub><mi>E</mi><mrow><mi>P</mi><mo>,</mo><mi>Ref</mi></mrow></msub><mfenced open="[" close="]"><mi>k</mi></mfenced></mrow></mfenced><mo>/</mo><msub><mi>E</mi><mrow><mi>P</mi><mo>,</mo><mi>Ref</mi></mrow></msub><mfenced open="[" close="]"><mi>k</mi></mfenced></mrow></mfenced>
 * </math></inlineequation>.
 * If the resulting noise loudness is negative, it is set to zero.
 */
void
peaq_mov_noise_loudness (PeaqModulationProcessor * const *ref_mod_proc,
                         PeaqModulationProcessor * const *test_mod_proc,
                         PeaqLevelAdapter * const *level,
                         PeaqMovAccum *mov_accum)
{
  guint c;

  for (c = 0; c < peaq_movaccum_get_channels (mov_accum); c++) {
    gdouble const *ref_excitation =
      peaq_leveladapter_get_adapted_ref (level[c]);
    gdouble const *test_excitation =
      peaq_leveladapter_get_adapted_test (level[c]);
    gdouble noise_loudness =
      calc_noise_loudness (1.5, 0.15, 0.5, 0., ref_mod_proc[c],
                           test_mod_proc[c], ref_excitation, test_excitation);
    peaq_movaccum_accumulate (mov_accum, c, noise_loudness, 1.);
  }
}

/**
 * peaq_mov_noise_loud_asym:
 * @ref_mod_proc: Modulation processors of the reference signal (one per
 * channel).
 * @test_mod_proc: Modulation processors of the test signal (one per channel).
 * @level: Level adapters (one per channel).
 * @mov_accum: Accumulator for the RmsNoiseLoudAsymA MOV.
 *
 * Calculates the RmsNoiseLoudAsymA model output variable as <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>NL</mi><mi>Asym</mi></msub><mo>=</mo><mi>NL</mi><mo>+</mo><mn>0.5</mn><mo>&sdot;</mo><mi>MC</mi>
 * </math></inlineequation> 
 * where
 * <informalequation><math xmlns="http://www.w3.org/1998/Math/MathML">
 *   <mi>NL</mi><mo>=</mo>
 *   <mfrac><mn>24</mn><mi>Z</mi></mfrac>
 *   <mo>&InvisibleTimes;</mo>
 *   <munderover>
 *     <mo>&sum;</mo>
 *     <mrow><mi>k</mi><mo>=</mo><mn>0</mn></mrow>
 *     <mrow><mi>Z</mi><mo>-</mo><mn>1</mn></mrow>
 *   </munderover>
 *   <msup>
 *     <mfenced>
 *       <mfrac>
 *         <mrow>
 *           <msub><mi>E</mi><mi>Thres</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *         </mrow>
 *         <mrow>
 *           <msub><mi>s</mi><mi>test</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *         </mrow>
 *       </mfrac>
 *     </mfenced>
 *     <mn>0.23</mn>
 *   </msup>
 *   <mfenced>
 *     <mrow>
 *       <msup>
 *         <mfenced>
 *           <mrow>
 *             <mn>1</mn>
 *             <mo>+</mo>
 *             <mfrac>
 *               <mrow>
 *                 <mi>max</mi>
 *                 <mfenced>
 *                   <mrow>
 *                     <msub><mi>s</mi><mi>test</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *                     <mo>&sdot;</mo>
 *                     <msub><mi>E</mi><mrow><mi>P</mi><mo>,</mo><mi>Test</mi></mrow></msub>
 *                     <mfenced open="[" close="]"><mi>k</mi></mfenced>
 *                     <mo>-</mo>
 *                     <msub><mi>s</mi><mi>ref</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *                     <mo>&sdot;</mo>
 *                     <msub><mi>E</mi><mrow><mi>P</mi><mo>,</mo><mi>Ref</mi></mrow></msub>
 *                     <mfenced open="[" close="]"><mi>k</mi></mfenced>
 *                   </mrow>
 *                   <mn>0</mn>
 *                 </mfenced>
 *               </mrow>
 *               <mrow>
 *                 <msub><mi>E</mi><mi>Thres</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *                 <mo>+</mo>
 *                 <msub><mi>s</mi><mi>ref</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *                 <mo>&sdot;</mo>
 *                 <msub><mi>E</mi><mrow><mi>P</mi><mo>,</mo><mi>Ref</mi></mrow></msub>
 *                 <mfenced open="[" close="]"><mi>k</mi></mfenced>
 *                 <mo>&sdot;</mo>
 *                 <mi>&beta;</mi><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *               </mrow>
 *             </mfrac>
 *           </mrow>
 *         </mfenced>
 *         <mn>0.23</mn>
 *       </msup>
 *       <mo>-</mo>
 *       <mn>1</mn>
 *     </mrow>
 *   </mfenced>
 * </math></informalequation> 
 * with <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>s</mi><mi>test</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced><mo>=</mo><mn>0.3</mn><mo>&sdot;</mo><msub><mi>Mod</mi><mi>test</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced><mo>+</mo><mn>1</mn>
 * </math></inlineequation>, <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>s</mi><mi>ref</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced><mo>=</mo><mn>0.3</mn><mo>&sdot;</mo><msub><mi>Mod</mi><mi>ref</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced><mo>+</mo><mn>1</mn>
 * </math></inlineequation>,
 * and <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><mi>&beta;</mi><mfenced open="[" close="]"><mi>k</mi></mfenced><mo>=</mo><mi>exp</mi><mfenced><mrow><mn>-2.5</mn><mo>&sdot;</mo><mfenced><mrow><msub><mi>E</mi><mrow><mi>P</mi><mo>,</mo><mi>Test</mi></mrow></msub><mfenced open="[" close="]"><mi>k</mi></mfenced><mo>-</mo><msub><mi>E</mi><mrow><mi>P</mi><mo>,</mo><mi>Ref</mi></mrow></msub><mfenced open="[" close="]"><mi>k</mi></mfenced></mrow></mfenced><mo>/</mo><msub><mi>E</mi><mrow><mi>P</mi><mo>,</mo><mi>Ref</mi></mrow></msub><mfenced open="[" close="]"><mi>k</mi></mfenced></mrow></mfenced>
 * </math></inlineequation> and
 * <informalequation><math xmlns="http://www.w3.org/1998/Math/MathML">
 *   <mi>MC</mi><mo>=</mo>
 *   <mfrac><mn>24</mn><mi>Z</mi></mfrac>
 *   <mo>&InvisibleTimes;</mo>
 *   <munderover>
 *     <mo>&sum;</mo>
 *     <mrow><mi>k</mi><mo>=</mo><mn>0</mn></mrow>
 *     <mrow><mi>Z</mi><mo>-</mo><mn>1</mn></mrow>
 *   </munderover>
 *   <msup>
 *     <mfenced>
 *       <mfrac>
 *         <mrow>
 *           <msub><mi>E</mi><mi>Thres</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *         </mrow>
 *         <mrow>
 *           <msub><mi>s</mi><mi>ref</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *         </mrow>
 *       </mfrac>
 *     </mfenced>
 *     <mn>0.23</mn>
 *   </msup>
 *   <mfenced>
 *     <mrow>
 *       <msup>
 *         <mfenced>
 *           <mrow>
 *             <mn>1</mn>
 *             <mo>+</mo>
 *             <mfrac>
 *               <mrow>
 *                 <mi>max</mi>
 *                 <mfenced>
 *                   <mrow>
 *                     <msub><mi>s</mi><mi>ref</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *                     <mo>&sdot;</mo>
 *                     <msub><mi>E</mi><mrow><mi>P</mi><mo>,</mo><mi>Ref</mi></mrow></msub>
 *                     <mfenced open="[" close="]"><mi>k</mi></mfenced>
 *                     <mo>-</mo>
 *                     <msub><mi>s</mi><mi>test</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *                     <mo>&sdot;</mo>
 *                     <msub><mi>E</mi><mrow><mi>P</mi><mo>,</mo><mi>Test</mi></mrow></msub>
 *                     <mfenced open="[" close="]"><mi>k</mi></mfenced>
 *                   </mrow>
 *                   <mn>0</mn>
 *                 </mfenced>
 *               </mrow>
 *               <mrow>
 *                 <msub><mi>E</mi><mi>Thres</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *                 <mo>+</mo>
 *                 <msub><mi>s</mi><mi>test</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *                 <mo>&sdot;</mo>
 *                 <msub><mi>E</mi><mrow><mi>P</mi><mo>,</mo><mi>Test</mi></mrow></msub>
 *                 <mfenced open="[" close="]"><mi>k</mi></mfenced>
 *                 <mo>&sdot;</mo>
 *                 <mi>&beta;</mi><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *               </mrow>
 *             </mfrac>
 *           </mrow>
 *         </mfenced>
 *         <mn>0.23</mn>
 *       </msup>
 *       <mo>-</mo>
 *       <mn>1</mn>
 *     </mrow>
 *   </mfenced>
 * </math></informalequation> 
 * with <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>s</mi><mi>test</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced><mo>=</mo><mn>0.15</mn><mo>&sdot;</mo><msub><mi>Mod</mi><mi>test</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced><mo>+</mo><mn>1</mn>
 * </math></inlineequation>, <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>s</mi><mi>ref</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced><mo>=</mo><mn>0.15</mn><mo>&sdot;</mo><msub><mi>Mod</mi><mi>ref</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced><mo>+</mo><mn>1</mn>
 * </math></inlineequation>,
 * and <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><mi>&beta;</mi><mfenced open="[" close="]"><mi>k</mi></mfenced><mo>=</mo><mi>exp</mi><mfenced><mrow><mn>-1.5</mn><mo>&sdot;</mo><mfenced><mrow><msub><mi>E</mi><mrow><mi>P</mi><mo>,</mo><mi>Ref</mi></mrow></msub><mfenced open="[" close="]"><mi>k</mi></mfenced><mo>-</mo><msub><mi>E</mi><mrow><mi>P</mi><mo>,</mo><mi>Test</mi></mrow></msub><mfenced open="[" close="]"><mi>k</mi></mfenced></mrow></mfenced><mo>/</mo><msub><mi>E</mi><mrow><mi>P</mi><mo>,</mo><mi>Test</mi></mrow></msub><mfenced open="[" close="]"><mi>k</mi></mfenced></mrow></mfenced>
 * </math></inlineequation>
 * where <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>E</mi><mi>Thres</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 * </math></inlineequation> 
 * is the internal ear noise as returned by peaq_earmodel_get_internal_noise(), <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>E</mi><mrow><mi>P</mi><mo>,</mo><mi>Ref</mi></mrow></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 * </math></inlineequation>
 * and <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>E</mi><mrow><mi>P</mi><mo>,</mo><mi>Test</mi></mrow></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 * </math></inlineequation>
 * are the spectrally adapted patterns of the reference and test signal as
 * returned by peaq_leveladapter_get_adapted_ref() and
 * peaq_leveladapter_get_adapted_test(), respectively, and <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>Mod</mi><mi>test</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 * </math></inlineequation>
 * and <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>Mod</mi><mi>ref</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 * </math></inlineequation>
 * are the modulation of the test and reference signal, respectively, as
* obtained with peaq_modulationprocessor_get_modulation()
 * If MC is negative, it is set to zero. Likewise, if NL is less than 0.1, it
 * is set to zero.
 *
 * Note: If #SWAP_MOD_PATTS_FOR_NOISE_LOUDNESS_MOVS is not set, <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>Mod</mi><mi>test</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 * </math></inlineequation>
 * and <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>Mod</mi><mi>ref</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 * </math></inlineequation> have to be exchanged in the calculation of MC.
 */
void
peaq_mov_noise_loud_asym (PeaqModulationProcessor * const *ref_mod_proc,
                          PeaqModulationProcessor * const *test_mod_proc,
                          PeaqLevelAdapter * const *level,
                          PeaqMovAccum *mov_accum)
{
  guint c;

  for (c = 0; c < peaq_movaccum_get_channels (mov_accum); c++) {
    gdouble const *ref_excitation =
      peaq_leveladapter_get_adapted_ref (level[c]);
    gdouble const *test_excitation =
      peaq_leveladapter_get_adapted_test (level[c]);
    gdouble noise_loudness =
      calc_noise_loudness (2.5, 0.3, 1., 0.1, ref_mod_proc[c],
                           test_mod_proc[c], ref_excitation, test_excitation);
#if defined(SWAP_MOD_PATTS_FOR_NOISE_LOUDNESS_MOVS) && SWAP_MOD_PATTS_FOR_NOISE_LOUDNESS_MOVS
    gdouble missing_components =
      calc_noise_loudness (1.5, 0.15, 1., 0., test_mod_proc[c],
                           ref_mod_proc[c], test_excitation, ref_excitation);
#else
    gdouble missing_components =
      calc_noise_loudness (1.5, 0.15, 1., 0., ref_mod_proc[c],
                           test_mod_proc[c], test_excitation, ref_excitation);
#endif
    peaq_movaccum_accumulate (mov_accum, c, noise_loudness, missing_components);
  }
}

/**
 * peaq_mov_lin_dist:
 * @ref_mod_proc: Modulation processors of the reference signal (one per
 * channel).
 * @test_mod_proc: Modulation processors of the test signal (one per channel).
 * @level: Level adapters (one per channel).
 * @state: States of the reference signal ear model (one per channels).
 * @mov_accum: Accumulator for the AvgLinDistA MOV.
 *
 * Calculates the AvgLinDistA model output variable as
 * <informalequation><math xmlns="http://www.w3.org/1998/Math/MathML">
 *   <mi>LD</mi><mo>=</mo>
 *   <mfrac><mn>24</mn><mi>Z</mi></mfrac>
 *   <mo>&InvisibleTimes;</mo>
 *   <munderover>
 *     <mo>&sum;</mo>
 *     <mrow><mi>k</mi><mo>=</mo><mn>0</mn></mrow>
 *     <mrow><mi>Z</mi><mo>-</mo><mn>1</mn></mrow>
 *   </munderover>
 *   <msup>
 *     <mfenced>
 *       <mfrac>
 *         <mrow>
 *           <msub><mi>E</mi><mi>Thres</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *         </mrow>
 *         <mrow>
 *           <msub><mi>s</mi><mi>test</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *         </mrow>
 *       </mfrac>
 *     </mfenced>
 *     <mn>0.23</mn>
 *   </msup>
 *   <mfenced>
 *     <mrow>
 *       <msup>
 *         <mfenced>
 *           <mrow>
 *             <mn>1</mn>
 *             <mo>+</mo>
 *             <mfrac>
 *               <mrow>
 *                 <mi>max</mi>
 *                 <mfenced>
 *                   <mrow>
 *                     <msub><mi>s</mi><mi>test</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *                     <mo>&sdot;</mo>
 *                     <msub><mi>E</mi><mi>Ref</mi></msub>
 *                     <mfenced open="[" close="]"><mi>k</mi></mfenced>
 *                     <mo>-</mo>
 *                     <msub><mi>s</mi><mi>ref</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *                     <mo>&sdot;</mo>
 *                     <msub><mi>E</mi><mrow><mi>P</mi><mo>,</mo><mi>Ref</mi></mrow></msub>
 *                     <mfenced open="[" close="]"><mi>k</mi></mfenced>
 *                   </mrow>
 *                   <mn>0</mn>
 *                 </mfenced>
 *               </mrow>
 *               <mrow>
 *                 <msub><mi>E</mi><mi>Thres</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *                 <mo>+</mo>
 *                 <msub><mi>s</mi><mi>ref</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *                 <mo>&sdot;</mo>
 *                 <msub><mi>E</mi><mrow><mi>P</mi><mo>,</mo><mi>Ref</mi></mrow></msub>
 *                 <mfenced open="[" close="]"><mi>k</mi></mfenced>
 *                 <mo>&sdot;</mo>
 *                 <mi>&beta;</mi><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *               </mrow>
 *             </mfrac>
 *           </mrow>
 *         </mfenced>
 *         <mn>0.23</mn>
 *       </msup>
 *       <mo>-</mo>
 *       <mn>1</mn>
 *     </mrow>
 *   </mfenced>
 * </math></informalequation> 
 * where <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>E</mi><mi>Thres</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 * </math></inlineequation> 
 * is the internal ear noise as returned by peaq_earmodel_get_internal_noise(), <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>E</mi><mrow><mi>P</mi><mo>,</mo><mi>Ref</mi></mrow></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 * </math></inlineequation>
 * are the spectrally adapted patterns of the reference signal as returned by
 * peaq_leveladapter_get_adapted_ref(), <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>E</mi><mi>Ref</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 * </math></inlineequation>
 * are the excitation patterns of the reference signal as returned by
 * peaq_earmodel_get_excitation(), <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>s</mi><mi>test</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced><mo>=</mo><msub><mi>s</mi><mi>ref</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced><mo>=</mo><mn>0.15</mn><mo>&sdot;</mo><msub><mi>Mod</mi><mi>ref</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced><mo>+</mo><mn>1</mn>
 * </math></inlineequation> 
 * are computed from the modulation <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>Mod</mi><mi>ref</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 * </math></inlineequation>
 * of the reference signal as obtained with
 * peaq_modulationprocessor_get_modulation() and <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><mi>&beta;</mi><mfenced open="[" close="]"><mi>k</mi></mfenced><mo>=</mo><mi>exp</mi><mfenced><mrow><mn>-1.5</mn><mo>&sdot;</mo><mfenced><mrow><msub><mi>E</mi><mi>Ref</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced><mo>-</mo><msub><mi>E</mi><mrow><mi>P</mi><mo>,</mo><mi>Ref</mi></mrow></msub><mfenced open="[" close="]"><mi>k</mi></mfenced></mrow></mfenced><mo>/</mo><msub><mi>E</mi><mrow><mi>P</mi><mo>,</mo><mi>Ref</mi></mrow></msub><mfenced open="[" close="]"><mi>k</mi></mfenced></mrow></mfenced>
 * </math></inlineequation>.
 * If the resulting linear distortion measure is negative, it is set to zero.
 *
 * Note: If #SWAP_MOD_PATTS_FOR_NOISE_LOUDNESS_MOVS is not set, <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>Mod</mi><mi>test</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 * </math></inlineequation>
 * is used to calculate <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>s</mi><mi>test</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 * </math></inlineequation>.
 */
void
peaq_mov_lin_dist (PeaqModulationProcessor * const *ref_mod_proc,
                   PeaqModulationProcessor * const *test_mod_proc,
                   PeaqLevelAdapter * const *level, const gpointer *state,
                   PeaqMovAccum *mov_accum)
{
  guint c;
  PeaqEarModel *ear_model =
    peaq_modulationprocessor_get_ear_model (ref_mod_proc[0]);

  for (c = 0; c < peaq_movaccum_get_channels (mov_accum); c++) {
    gdouble const *ref_adapted_excitation =
      peaq_leveladapter_get_adapted_ref (level[c]);
    gdouble const *ref_excitation =
      peaq_earmodel_get_excitation (ear_model, state[c]);
#if defined(SWAP_MOD_PATTS_FOR_NOISE_LOUDNESS_MOVS) && SWAP_MOD_PATTS_FOR_NOISE_LOUDNESS_MOVS
    gdouble noise_loudness =
      calc_noise_loudness (1.5, 0.15, 1., 0., ref_mod_proc[c],
                           ref_mod_proc[c], ref_adapted_excitation,
                           ref_excitation);
#else
    gdouble noise_loudness =
      calc_noise_loudness (1.5, 0.15, 1., 0., ref_mod_proc[c],
                           test_mod_proc[c], ref_adapted_excitation,
                           ref_excitation);
#endif
    peaq_movaccum_accumulate (mov_accum, c, noise_loudness, 1.);
  }
}

static gdouble
calc_noise_loudness (gdouble alpha, gdouble thres_fac, gdouble S0,
                     gdouble NLmin,
                     PeaqModulationProcessor const *ref_mod_proc,
                     PeaqModulationProcessor const *test_mod_proc,
                     gdouble const *ref_excitation,
                     gdouble const *test_excitation)
{
  guint i;
  gdouble noise_loudness = 0.;
  PeaqEarModel *ear_model =
    peaq_modulationprocessor_get_ear_model (ref_mod_proc);
  guint band_count = peaq_earmodel_get_band_count (ear_model);
  gdouble const *ref_modulation =
    peaq_modulationprocessor_get_modulation (ref_mod_proc);
  gdouble const *test_modulation =
    peaq_modulationprocessor_get_modulation (test_mod_proc);
  for (i = 0; i < band_count; i++) {
    /* (67) in [BS1387] */
    gdouble sref = thres_fac * ref_modulation[i] + S0;
    gdouble stest = thres_fac * test_modulation[i] + S0;
    gdouble ethres = peaq_earmodel_get_internal_noise (ear_model, i);
    gdouble ep_ref = ref_excitation[i];
    gdouble ep_test = test_excitation[i];
    /* (68) in [BS1387] */
    gdouble beta = exp (-alpha * (ep_test - ep_ref) / ep_ref);
    /* (66) in [BS1387] */
    noise_loudness += pow (ethres / stest, 0.23) *
      (pow (1. + MAX (stest * ep_test - sref * ep_ref, 0.) /
	    (ethres + sref * ep_ref * beta), 0.23) - 1.);
  }
  noise_loudness *= 24. / band_count;
  if (noise_loudness < NLmin)
    noise_loudness = 0.;
  return noise_loudness;
}

/**
 * peaq_mov_bandwidth:
 * @ref_state: State of the reference signal #PeaqFFTEarModel.
 * @test_state: State of the test signal #PeaqFFTEarModel.
 * @mov_accum_ref: Accumulator for the BandwidthRefB MOV.
 * @mov_accum_test: Accumulator for the BandwidthTestB MOV.
 *
 * Calculates the bandwidth based MOVs as described in section 4.4
 * of <xref linkend="BS1387" />. The power spectra <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msup><mfenced open="|" close="|"><mrow><msub><mi>F</mi><mi>Ref</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced></mrow></mfenced><mn>2</mn></msup>
 * </math></inlineequation>
 * and <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msup><mfenced open="|" close="|"><mrow><msub><mi>F</mi><mi>Test</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced></mrow></mfenced><mn>2</mn></msup>
 * </math></inlineequation>
 * are obtained from @ref_state and @test_state, respectively, using
 * peaq_fftearmodel_get_power_spectrum().The first step is to determine the
 * zero threshold <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><munder><mi>max</mi><mrow><mn>921</mn><mo>&le;</mo><mi>k</mi><mo>&le;</mo><mn>1023</mn></mrow></munder><msup><mfenced open="|" close="|"><mrow><msub><mi>F</mi><mi>Ref</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced></mrow></mfenced><mn>2</mn></msup>
 * </math></inlineequation>.
 * The reference signal bandwidth is then determined as the largest <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><mi>k</mi>
 * </math></inlineequation>
 * such that <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msup><mfenced open="|" close="|"><mrow><msub><mi>F</mi><mi>Ref</mi></msub><mfenced open="[" close="]"><mrow><mi>k</mi><mo>-</mo><mn>1</mn></mrow></mfenced></mrow></mfenced><mn>2</mn></msup>
 * </math></inlineequation>
 * is 10 dB above the zero threshold. Likewise, the test signal bandwidth is
 * then determined as the largest <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><mi>k</mi>
 * </math></inlineequation>
 * smaller than the reference signal bandwidth such that <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msup><mfenced open="|" close="|"><mrow><msub><mi>F</mi><mi>Test</mi></msub><mfenced open="[" close="]"><mrow><mi>k</mi><mo>-</mo><mn>1</mn></mrow></mfenced></mrow></mfenced><mn>2</mn></msup>
 * </math></inlineequation>
 * is 5 dB above the zero threshold. If no frequency bin is above the zero
 * threshold, the respective bandwidth is set to zero. The resulting bandwidths
 * are accumulated to @mov_accum_ref and @mov_accum_test only if the reference
 * bandwidth is greater than 346.
 */
void
peaq_mov_bandwidth (const gpointer *ref_state, const gpointer *test_state,
                    PeaqMovAccum *mov_accum_ref, PeaqMovAccum *mov_accum_test)
{
  guint c;

  for (c = 0; c < peaq_movaccum_get_channels (mov_accum_ref); c++) {
    guint i;
    gdouble const *ref_power_spectrum =
      peaq_fftearmodel_get_power_spectrum (ref_state[c]);
    gdouble const *test_power_spectrum =
      peaq_fftearmodel_get_power_spectrum (test_state[c]);
    gdouble zero_threshold = test_power_spectrum[921];
    for (i = 922; i < 1024; i++)
      if (test_power_spectrum[i] >= zero_threshold)
        zero_threshold = test_power_spectrum[i];
    guint bw_ref = 0;
    for (i = 921; i > 0; i--)
      if (ref_power_spectrum[i - 1] > 10. * zero_threshold) {
        bw_ref = i;
        break;
      }
    if (bw_ref > 346) {
      guint bw_test = 0;
      for (i = bw_ref; i > 0; i--)
        if (test_power_spectrum[i - 1] >=
            FIVE_DB_POWER_FACTOR * zero_threshold) {
          bw_test = i;
          break;
        }
      peaq_movaccum_accumulate (mov_accum_ref, c, bw_ref, 1.);
      peaq_movaccum_accumulate (mov_accum_test, c, bw_test, 1.);
    }
  }
}

/**
 * peaq_mov_nmr:
 * @ear_model: The underlying FFT based ear model to which @ref_state and
 * @test_state belong.
 * @ref_state: Ear model states for the reference signal.
 * @test_state: Ear model states for the test signal.
 * @mov_accum_nmr: Accumulator for the Total NMRB or Segmental NMRB MOVs.
 * @mov_accum_rel_dist_frames: Accumulator for the Relative Disturbed FramesB
 * MOV or NULL.
 *
 * Calculates the noise-to-mask ratio based model output variables as described
 * in sections 4.5 and 4.6 of <xref linkend="BS1387" />. From the weighted power
 * spectra <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msup><mfenced open="|" close="|"><mrow><msub><mi>F</mi><mrow><mi>e</mi><mo>,</mo><mi>Ref</mi></mrow></msub><mfenced open="[" close="]"><mi>k</mi></mfenced></mrow></mfenced><mn>2</mn></msup>
 * </math></inlineequation>
 * and <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msup><mfenced open="|" close="|"><mrow><msub><mi>F</mi><mrow><mi>e</mi><mo>,</mo><mi>Test</mi></mrow></msub><mfenced open="[" close="]"><mi>k</mi></mfenced></mrow></mfenced><mn>2</mn></msup>
 * </math></inlineequation>
 * obtained with
 * peaq_fftearmodel_get_weighted_power_spectrum() from @ref_state and
 * @test_state, respectively, the noise power spectrum
 * <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML">
 *   <msup>
 *     <mfenced open="|" close="|">
 *       <mrow>
 *         <msub><mi>F</mi><mi>noise</mi></msub>
 *         <mfenced open="[" close="]"><mi>k</mi></mfenced>
 *       </mrow>
 *     </mfenced>
 *     <mn>2</mn>
 *   </msup>
 *   <mo>=</mo>
 *   <msup>
 *     <mfenced open="|" close="|">
 *       <mrow>
 *         <mfenced open="|" close="|">
 *           <msub><mi>F</mi><mrow><mi>e</mi><mo>,</mo><mi>Test</mi></mrow></msub>
 *           <mfenced open="[" close="]"><mi>k</mi></mfenced>
 *         </mfenced>
 *         <mo>-</mo>
 *         <mfenced open="|" close="|">
 *           <msub><mi>F</mi><mrow><mi>e</mi><mo>,</mo><mi>Ref</mi></mrow></msub>
 *           <mfenced open="[" close="]"><mi>k</mi></mfenced>
 *         </mfenced>
 *       </mrow>
 *     </mfenced>
 *     <mn>2</mn>
 *   </msup>
 * </math></inlineequation>
 * is calculated as
 * <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML">
 *   <msup>
 *     <mfenced open="|" close="|">
 *       <mrow>
 *         <msub><mi>F</mi><mi>noise</mi></msub>
 *         <mfenced open="[" close="]"><mi>k</mi></mfenced>
 *       </mrow>
 *     </mfenced>
 *     <mn>2</mn>
 *   </msup>
 *   <mo>=</mo>
 *   <msup>
 *     <mfenced open="|" close="|">
 *       <mrow>
 *         <msub><mi>F</mi><mrow><mi>e</mi><mo>,</mo><mi>Ref</mi></mrow></msub>
 *         <mfenced open="[" close="]"><mi>k</mi></mfenced>
 *       </mrow>
 *     </mfenced>
 *     <mn>2</mn>
 *   </msup>
 *   <mo>-</mo>
 *   <mn>2</mn>
 *   <mo>&InvisibleTimes;</mo>
 *   <msqrt>
 *     <msup>
 *       <mfenced open="|" close="|">
 *         <mrow>
 *           <msub><mi>F</mi><mrow><mi>e</mi><mo>,</mo><mi>Ref</mi></mrow></msub>
 *           <mfenced open="[" close="]"><mi>k</mi></mfenced>
 *         </mrow>
 *       </mfenced>
 *       <mn>2</mn>
 *     </msup>
 *     <mo>&InvisibleTimes;</mo>
 *     <msup>
 *       <mfenced open="|" close="|">
 *         <mrow>
 *           <msub><mi>F</mi><mrow><mi>e</mi><mo>,</mo><mi>Ref</mi></mrow></msub>
 *           <mfenced open="[" close="]"><mi>k</mi></mfenced>
 *         </mrow>
 *       </mfenced>
 *       <mn>2</mn>
 *     </msup>
 *   </msqrt>
 *   <mo>+</mo>
 *   <msup>
 *     <mfenced open="|" close="|">
 *       <mrow>
 *         <msub><mi>F</mi><mrow><mi>e</mi><mo>,</mo><mi>Ref</mi></mrow></msub>
 *         <mfenced open="[" close="]"><mi>k</mi></mfenced>
 *       </mrow>
 *     </mfenced>
 *     <mn>2</mn>
 *   </msup>
 * </math></inlineequation>
 * and grouped into bands using peaq_fftearmodel_group_into_bands() to obtain
 * the noise patterns <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>P</mi><mi>noise</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 * </math></inlineequation>.
 * The mask pattern <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><mi>M</mi><mfenced open="[" close="]"><mi>k</mi></mfenced>
 * </math></inlineequation>
 * is calculated from the excitation pattern of the reference signal as
 * obtained by peaq_earmodel_get_excitation() from @ref_state by dividing it by
 * the masking difference as returned by
 * peaq_fftearmodel_get_masking_difference(). From these, the noise-to-mask
 * ratio is calculated as
 * <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML">
 *   <mi>NMR</mi>
 *   <mo>=</mo>
 *   <mfrac><mn>1</mn><mi>Z</mi></mfrac>
 *   <mo>&InvisibleTimes;</mo>
 *   <munderover>
 *     <mo>&sum;</mo>
 *     <mrow><mi>k</mi><mo>=</mo><mn>0</mn></mrow>
 *     <mrow><mi>Z</mi><mo>-</mo><mn>1</mn></mrow>
 *   </munderover>
 *   <mfrac>
 *     <mrow>
 *       <msub><mi>P</mi><mi>noise</mi></msub>
 *       <mfenced open="[" close="]"><mi>k</mi></mfenced>
 *     </mrow>
 *     <mrow>
 *       <mi>M</mi><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *     </mrow>
 *   </mfrac>
 * </math></inlineequation>
 * where <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><mi>Z</mi>
 * </math></inlineequation>
 * denotes the number of bands. If @mov_accum_nmr is set to #MODE_AVG_LOG, the
 * NMR is directly accumulated (used for Total NMRB), otherwise, it is
 * converted to dB-scale first (used for Segmental NMRB).
 *
 * If @mov_accum_rel_dist_frames is not NULL, in addition, the frames where
 * <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML">
 *   <munder>
 *     <mi>max</mi>
 *     <mrow><mi>k</mi><mo>&lt;</mo><mi>Z</mi></mrow>
 *   </munder>
 *   <mfrac>
 *     <mrow>
 *       <msub><mi>P</mi><mi>noise</mi></msub>
 *       <mfenced open="[" close="]"><mi>k</mi></mfenced>
 *     </mrow>
 *     <mrow>
 *       <mi>M</mi><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *     </mrow>
 *   </mfrac>
 * </math></inlineequation>
 * exceeds 1.41253754462275 (corresponding to 1.5 dB) are counted by
 * accumlating one for frames that do exceed the threshold, a zero for those
 * that do not.
 */
void
peaq_mov_nmr (PeaqFFTEarModel const *ear_model, const gpointer *ref_state,
              const gpointer *test_state, PeaqMovAccum *mov_accum_nmr,
              PeaqMovAccum *mov_accum_rel_dist_frames)
{
  guint c;
  guint band_count = peaq_earmodel_get_band_count (PEAQ_EARMODEL (ear_model));
  guint frame_size = peaq_earmodel_get_frame_size (PEAQ_EARMODEL (ear_model));
  gdouble const *masking_difference = 
    peaq_fftearmodel_get_masking_difference (ear_model);
  for (c = 0; c < peaq_movaccum_get_channels (mov_accum_nmr); c++) {
    guint i;
    gdouble const *ref_excitation =
      peaq_earmodel_get_excitation (PEAQ_EARMODEL (ear_model), ref_state[c]);
    gdouble nmr = 0.;
    gdouble nmr_max = 0.;
    gdouble *noise_in_bands = g_newa (gdouble, band_count);
    gdouble const *ref_weighted_power_spectrum =
      peaq_fftearmodel_get_weighted_power_spectrum (ref_state[c]);
    gdouble const *test_weighted_power_spectrum =
      peaq_fftearmodel_get_weighted_power_spectrum (test_state[c]);
    gdouble noise_spectrum[1025];

    for (i = 0; i < frame_size / 2 + 1; i++)
      noise_spectrum[i] =
        ref_weighted_power_spectrum[i] -
        2 * sqrt (ref_weighted_power_spectrum[i] *
                  test_weighted_power_spectrum[i]) +
        test_weighted_power_spectrum[i];

    peaq_fftearmodel_group_into_bands (ear_model, noise_spectrum,
                                       noise_in_bands);

    for (i = 0; i < band_count; i++) {
      /* (26) in [BS1387] */
      gdouble mask = ref_excitation[i] / masking_difference[i];
      /* (70) in [BS1387], except for conversion to dB in the end */
      gdouble curr_nmr = noise_in_bands[i] / mask;
      nmr += curr_nmr;
      /* for Relative Disturbed Frames */
      if (curr_nmr > nmr_max)
        nmr_max = curr_nmr;
    }
    nmr /= band_count;

    if (peaq_movaccum_get_mode (mov_accum_nmr) == MODE_AVG_LOG)
      peaq_movaccum_accumulate (mov_accum_nmr, c, nmr, 1.);
    else
      peaq_movaccum_accumulate (mov_accum_nmr, c, 10. * log10 (nmr), 1.);
    if (mov_accum_rel_dist_frames)
      peaq_movaccum_accumulate (mov_accum_rel_dist_frames, c,
                                nmr_max > ONE_POINT_FIVE_DB_POWER_FACTOR ? 1. : 0., 1.);
  }
}

/**
 * peaq_mov_prob_detect:
 * @ear_model: The underlying ear model to which @ref_state and
 * @test_state belong.
 * @ref_state: Ear model states for the reference signal.
 * @test_state: Ear model states for the test signal.
 * @channels: Number of audio channels being processed.
 * @mov_accum_adb: Accumulator for the ADBB MOV.
 * @mov_accum_mfpd: Accumulator for the MFPDB MOV.
 *
 * Calculates the detection probability based model output variables as described in 
 * in section 4.7 of <xref linkend="BS1387" />. The excitation patterns <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><mrow><msub><mi>E</mi><mi>Ref</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced></mrow>
 * </math></inlineequation>
 * and <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><mrow><msub><mi>E</mi><mi>Test</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced></mrow> 
 * </math></inlineequation>
 * are converted to dB as <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><mrow><msub><mover><mi>E</mi><mo>~</mo></mover><mi>Ref</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced><mo>=</mo><mn>10</mn><mo>&InvisibleTimes;</mo><msub><mi>log</mi><mn>10</mn></msub><mfenced><mrow><msub><mi>E</mi><mi>Ref</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced></mrow></mfenced></mrow>
 * </math></inlineequation>
 * and <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><mrow><msub><mover><mi>E</mi><mo>~</mo></mover><mi>Test</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced><mo>=</mo><mn>10</mn><mo>&InvisibleTimes;</mo><msub><mi>log</mi><mn>10</mn></msub><mfenced><mrow><msub><mi>E</mi><mi>Test</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced></mrow></mfenced></mrow>
 * </math></inlineequation> 
 * from which the asymmetric average exciation <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><mrow><mi>L</mi><mfenced open="[" close="]"><mi>k</mi></mfenced><mo>=</mo><mn>0.3</mn><mo>&InvisibleTimes;</mo><mi>max</mi><mfenced><mrow><msub><mover><mi>E</mi><mo>~</mo></mover><mi>Ref</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced></mrow><mrow><msub><mover><mi>E</mi><mo>~</mo></mover><mi>Test</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced></mrow></mfenced><mo>+</mo><mn>0.7</mn><mo>&InvisibleTimes;</mo><msub><mover><mi>E</mi><mo>~</mo></mover><mi>Test</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced></mrow>
 * </math></inlineequation> 
 * is computed. This is then used to determine the effective detection step size
 * <informalequation>
 *   <math xmlns="http://www.w3.org/1998/Math/MathML">
 *     <mrow>
 *       <mi>s</mi><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *       <mo>=</mo>
 *       <mn>5.95072</mn><mo>&sdot;</mo>
 *       <msup>
 *         <mfenced>
 *           <mrow>
 *             <mn>6.39468</mn>
 *             <mo>/</mo>
 *             <mi>L</mi><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *           </mrow>
 *         </mfenced>
 *         <mn>1.71332</mn>
 *       </msup>
 *       <mo>+</mo>
 *       <mn>9.01033</mn><mo>&times;</mo><msup><mn>10</mn><mn>-11</mn></msup>
 *       <mo>&sdot;</mo>
 *       <msup>
 *         <mrow><mi>L</mi><mfenced open="[" close="]"><mi>k</mi></mfenced></mrow>
 *         <mn>4</mn>
 *       </msup>
 *       <mo>+</mo>
 *       <mn>5.05622</mn><mo>&times;</mo><msup><mn>10</mn><mn>-6</mn></msup>
 *       <mo>&sdot;</mo>
 *       <msup>
 *         <mrow><mi>L</mi><mfenced open="[" close="]"><mi>k</mi></mfenced></mrow>
 *         <mn>3</mn>
 *       </msup>
 *       <mo>-</mo>
 *       <mn>0.00102438</mn>
 *       <mo>&sdot;</mo>
 *       <msup>
 *         <mrow><mi>L</mi><mfenced open="[" close="]"><mi>k</mi></mfenced></mrow>
 *         <mn>2</mn>
 *       </msup>
 *       <mo>+</mo>
 *       <mn>0.0550197</mn>
 *       <mo>&sdot;</mo>
 *       <mi>L</mi><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *       <mo>-</mo>
 *       <mn>0.198719</mn>
 *     </mrow>
 *   </math>
 * </informalequation> 
 * if <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><mrow><mi>L</mi><mfenced open="[" close="]"><mi>k</mi></mfenced><mo>&gt;</mo><mn>0</mn></mrow>
 * </math></inlineequation>, <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><mrow><mi>s</mi><mfenced open="[" close="]"><mi>k</mi></mfenced><mo>=</mo><msup><mn>10</mn><mn>30</mn></msup></mrow>
 * </math></inlineequation>
 * otherwise. For every channel <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><mi>c</mi>
 * </math></inlineequation>, the probability of detection is then given by
 * <informalequation>
 *   <math xmlns="http://www.w3.org/1998/Math/MathML">
 *     <mrow>
 *       <msub><mi>p</mi><mi>c</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *       <mo>=</mo>
 *       <mn>1</mn>
 *       <mo>-</mo>
 *       <msup>
 *         <mn>0.5</mn>
 *         <msup>
 *           <mfenced>
 *             <mrow>
 *               <mfenced>
 *                 <mrow>
 *                   <msub><mover><mi>E</mi><mo>~</mo></mover><mi>Ref</mi></msub>
 *                   <mfenced open="[" close="]"><mi>k</mi></mfenced>
 *                   <mo>-</mo>
 *                   <msub><mover><mi>E</mi><mo>~</mo></mover><mi>Test</mi></msub>
 *                   <mfenced open="[" close="]"><mi>k</mi></mfenced>
 *                 </mrow>
 *               </mfenced>
 *               <mo>/</mo> 
 *               <mi>s</mi><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *             </mrow>
 *           </mfenced>
 *           <mi>b</mi>
 *         </msup>
 *       </msup>
 *     </mrow>
 *    <mspace width="2em" />
 *    <mtext> where </mtext>
 *    <mspace width="2em" />
 *    <mi>b</mi>
 *    <mo>=</mo>
 *    <mfenced open="{" close="">
 *      <mtable>
 *        <mtr>
 *          <mtd><mn>4</mn></mtd>
 *          <mtd>
 *            <mtext>if </mtext>
 *            <msub><mover><mi>E</mi><mo>~</mo></mover><mi>Ref</mi></msub>
 *            <mfenced open="[" close="]"><mi>k</mi></mfenced>
 *            <mo>&gt;</mo>
 *            <msub><mover><mi>E</mi><mo>~</mo></mover><mi>Test</mi></msub>
 *            <mfenced open="[" close="]"><mi>k</mi></mfenced>
 *          </mtd>
 *        </mtr>
 *        <mtr><mtd><mn>6</mn></mtd><mtd><mtext>else</mtext></mtd></mtr>
 *      </mtable>
 *    </mfenced>
 *   </math>
 * </informalequation> 
 * and the total number of steps above the threshold as
 * <informalequation>
 *   <math xmlns="http://www.w3.org/1998/Math/MathML">
 *     <mrow>
 *       <msub><mi>q</mi><mi>c</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *       <mo>=</mo>
 *       <mfrac>
 *         <mfenced open="|" close="|">
 *           <mrow>
 *             <mi>INT</mi>
 *             <mfenced>
 *               <mrow>
 *                 <msub><mover><mi>E</mi><mo>~</mo></mover><mi>Ref</mi></msub>
 *                 <mfenced open="[" close="]"><mi>k</mi></mfenced>
 *                 <mo>-</mo>
 *                 <msub><mover><mi>E</mi><mo>~</mo></mover><mi>Test</mi></msub>
 *                 <mfenced open="[" close="]"><mi>k</mi></mfenced>
 *               </mrow>
 *             </mfenced>
 *           </mrow>
 *         </mfenced>
 *         <mrow><mi>s</mi><mfenced open="[" close="]"><mi>k</mi></mfenced></mrow>
 *       </mfrac>
 *     </mrow>
 *   </math>.
 * </informalequation>
 * The binaural values are then given as the respective maxima over all channels <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>p</mi><mi>bin</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced><mo>=</mo><munder><mi>max</mi><mi>c</mi></munder><mfenced><mrow><msub><mi>p</mi><mi>c</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced></mrow></mfenced>
 * </math></inlineequation> and <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>q</mi><mi>bin</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced><mo>=</mo><munder><mi>max</mi><mi>c</mi></munder><mfenced><mrow><msub><mi>q</mi><mi>c</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced></mrow></mfenced>
 * </math></inlineequation>, from which the total probability of detection is computed as
 * <informalequation>
 *   <math xmlns="http://www.w3.org/1998/Math/MathML">
 *     <mrow>
 *       <msub><mi>P</mi><mi>bin</mi></msub>
 *       <mo>=</mo>
 *       <mn>1</mn>
 *       <mo>-</mo>
 *       <munderover>
 *         <mo>&prod;</mo>
 *         <mrow><mi>k</mi><mo>=</mo><mn>0</mn></mrow>
 *         <mrow><mi>Z</mi><mo>-</mo><mn>1</mn></mrow>
 *       </munderover>
 *       <mfenced>
 *         <mrow>
 *           <mn>1</mn>
 *           <mo>-</mo>
 *           <msub><mi>p</mi><mi>bin</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *         </mrow>
 *       </mfenced>
 *     </mrow>
 *   </math>
 * </informalequation>
 * and the total number of steps above the threshold as
 * <informalequation>
 *   <math xmlns="http://www.w3.org/1998/Math/MathML">
 *     <mrow>
 *       <msub><mi>Q</mi><mi>bin</mi></msub>
 *       <mo>=</mo>
 *       <munderover>
 *         <mo>&sum;</mo>
 *         <mrow><mi>k</mi><mo>=</mo><mn>0</mn></mrow>
 *         <mrow><mi>Z</mi><mo>-</mo><mn>1</mn></mrow>
 *       </munderover>
 *       <msub><mi>q</mi><mi>bin</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced>
 *     </mrow>
 *   </math>
 * </informalequation>
 * where <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><mi>Z</mi>
 * </math></inlineequation> denotes the number of bands. The total probaility of
 * detection is accumulated in @mov_accum_mfpd, which should be set to
 * #MODE_FILTERED_MAX, and for frames with <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><msub><mi>P</mi><mi>bin</mi></msub><mo>&gt;</mo><mn>0.5</mn>
 * </math></inlineequation>, the total number of steps above the threshold is
 * accumulated in @mov_accum_adb, which should be set to #MODE_ADB.
 */
void
peaq_mov_prob_detect (PeaqEarModel const *ear_model, const gpointer *ref_state,
                      const gpointer *test_state, guint channels,
                      PeaqMovAccum *mov_accum_adb,
                      PeaqMovAccum *mov_accum_mfpd)
{
  guint c;
  guint i;
  guint band_count = peaq_earmodel_get_band_count (ear_model);
  gdouble binaural_detection_probability = 1.;
  gdouble binaural_detection_steps = 0.;
  for (i = 0; i < band_count; i++) {
    gdouble detection_probability = 0.;
    gdouble detection_steps = 0.;
    for (c = 0; c < channels; c++) {
      gdouble const *ref_excitation =
        peaq_earmodel_get_excitation (ear_model, ref_state[c]);
      gdouble const *test_excitation =
        peaq_earmodel_get_excitation (ear_model, test_state[c]);
      gdouble eref_db = 10. * log10 (ref_excitation[i]);
      gdouble etest_db = 10. * log10 (test_excitation[i]);
      /* (73) in [BS1387] */
      gdouble l = 0.3 * MAX (eref_db, etest_db) + 0.7 * etest_db;
      /* (74) in [BS1387] */
      gdouble s = l > 0. ? 5.95072 * pow (6.39468 / l, 1.71332) +
        9.01033e-11 * pow (l, 4.) + 5.05622e-6 * pow (l, 3.) -
        0.00102438 * l * l + 0.0550197 * l - 0.198719 : 1e30;
      /* (75) in [BS1387] */
      gdouble e = eref_db - etest_db;
      gdouble b = eref_db > etest_db ? 4. : 6.;
      /* (76) and (77) in [BS1387] simplify to this */
      gdouble pc = 1. - pow (0.5, pow (e / s, b));
      /* (78) in [BS1387] */
#if defined(USE_FLOOR_FOR_STEPS_ABOVE_THRESHOLD) && USE_FLOOR_FOR_STEPS_ABOVE_THRESHOLD
      gdouble qc = fabs (floor(e)) / s;
#else
      gdouble qc = fabs (trunc(e)) / s;
#endif
      if (pc > detection_probability)
        detection_probability = pc;
      if (c == 0 || qc > detection_steps)
        detection_steps = qc;
    }
    binaural_detection_probability *= 1. - detection_probability;
    binaural_detection_steps += detection_steps;
  }
  binaural_detection_probability = 1. - binaural_detection_probability;
  if (binaural_detection_probability > 0.5) {
    peaq_movaccum_accumulate (mov_accum_adb, 0,
                              binaural_detection_steps, 1.);
  }
  peaq_movaccum_accumulate (mov_accum_mfpd, 0,
                            binaural_detection_probability, 1.);
}

static void
do_xcorr(gdouble const* d, gdouble * c)
{
  static GstFFTF64 *correlator_fft = NULL;
  static GstFFTF64 *correlator_inverse_fft = NULL;
  if (correlator_fft == NULL)
    correlator_fft = gst_fft_f64_new (2 * MAXLAG, FALSE);
  if (correlator_inverse_fft == NULL)
    correlator_inverse_fft = gst_fft_f64_new (2 * MAXLAG, TRUE);
  /*
   * the follwing uses an equivalent computation in the frequency domain to
   * determine the correlation like function:
   * for (i = 0; i < MAXLAG; i++) {
   *   c[i] = 0;
   *   for (k = 0; k < MAXLAG; k++)
   *     c[i] += d[k] * d[k + i];
   * }
  */
  guint k;
  gdouble timedata[2 * MAXLAG];
  GstFFTF64Complex freqdata1[MAXLAG + 1];
  GstFFTF64Complex freqdata2[MAXLAG + 1];
  memcpy (timedata, d, 2 * MAXLAG * sizeof(gdouble));
  gst_fft_f64_fft (correlator_fft, timedata, freqdata1);
  memset (timedata + MAXLAG, 0, MAXLAG * sizeof(gdouble));
  gst_fft_f64_fft (correlator_fft, timedata, freqdata2);
  for (k = 0; k < MAXLAG + 1; k++) {
    /* multiply freqdata1 with the conjugate of freqdata2 */
    gdouble r = (freqdata1[k].r * freqdata2[k].r
                 + freqdata1[k].i * freqdata2[k].i) / (2 * MAXLAG);
    gdouble i = (freqdata2[k].r * freqdata1[k].i
                 - freqdata1[k].r * freqdata2[k].i) / (2 * MAXLAG);
    freqdata1[k].r = r;
    freqdata1[k].i = i;
  }
  gst_fft_f64_inverse_fft (correlator_inverse_fft, freqdata1, timedata);
  memcpy (c, timedata, MAXLAG * sizeof(gdouble));
}

/**
 * peaq_mov_ehs:
 * @ear_model: The underlying ear model to which @ref_state and
 * @test_state belong.
 * @ref_state: Ear model states for the reference signal.
 * @test_state: Ear model states for the test signal.
 * @mov_accum: Accumulator for the EHSB MOV.
 *
 * Calculates the error harmonic structure based model output variable as
 * described in section 4.8 of <xref linkend="BS1387" /> with the
 * interpretations of <xref linkend="Kabal03" />. The error harmonic structure
 * is computed based on the difference of the logarithms of the weighted power
 * spectra <inlineequation><math xmlns="http://www.w3.org/1998/Math/MathML"><mrow><msub><mi>F</mi><mi>e</mi></msub><mfenced open="[" close="]"><mi>k</mi></mfenced></mrow>
 * </math></inlineequation>
 * for test and reference signal. The autocorrelation of this difference is
 * then windowed and Fourier-transformed. In the resulting cepstrum-like data,
 * the height of the maximum peak after the first valley is the EHSB model
 * output variable which is accumulated in @mov_accum.
 *
 * Two aspects in which <xref linkend="Kabal03" /> suggests to not strictly
 * follow <xref linkend="BS1387" /> can be controlled by compile-time switches:
 * * #CENTER_EHS_CORRELATION_WINDOW controls whether the window to apply is
 *   centered around lag 0 of the correlation as suggested in <xref
 *   linkend="Kabal03" /> (if unset or set to false) or centered around the
 *   middle of the correlation (if set to true).
 * * #EHS_SUBTRACT_DC_BEFORE_WINDOW controls whether the average is subtracted
 *   before windowing as suggested in <xref linkend="Kabal03" /> or afterwards.
 */
void
peaq_mov_ehs (PeaqEarModel const *ear_model, gpointer *ref_state,
              gpointer *test_state, PeaqMovAccum *mov_accum)
{
  guint i;
  guint chan;

  static GstFFTF64 *correlation_fft = NULL;
  static gdouble *correlation_window = NULL;
  if (correlation_fft == NULL)
    correlation_fft = gst_fft_f64_new (MAXLAG, FALSE);
  if (correlation_window == NULL) {
    /* centering the window of the correlation in the EHS computation at lag
     * zero (as considered in [Kabal03] to be more reasonable) degrades
     * conformance */
    correlation_window = g_new (gdouble, MAXLAG);
    for (i = 0; i < MAXLAG; i++)
#if defined(CENTER_EHS_CORRELATION_WINDOW) && CENTER_EHS_CORRELATION_WINDOW
      correlation_window[i] = 0.81649658092773 *
        (1 + cos (2 * M_PI * i / (2 * MAXLAG - 1))) / MAXLAG;
#else
    correlation_window[i] = 0.81649658092773 *
      (1 - cos (2 * M_PI * i / (MAXLAG - 1))) / MAXLAG;
#endif
  }

  gint channels = peaq_movaccum_get_channels(mov_accum);

  guint frame_size = peaq_earmodel_get_frame_size (ear_model);
  gboolean ehs_valid = FALSE;
  for (chan = 0; chan < channels; chan++) {
    if (peaq_fftearmodel_is_energy_threshold_reached (ref_state[chan]) ||
        peaq_fftearmodel_is_energy_threshold_reached (test_state[chan]))
    ehs_valid = TRUE;
  }
  if (!ehs_valid)
    return;

  for (chan = 0; chan < channels; chan++) {
    gdouble const *ref_power_spectrum =
      peaq_fftearmodel_get_weighted_power_spectrum (ref_state[chan]);
    gdouble const *test_power_spectrum =
      peaq_fftearmodel_get_weighted_power_spectrum (test_state[chan]);

    gdouble *d = g_newa (gdouble, frame_size / 2 + 1);
    gdouble c[MAXLAG];
    gdouble d0;
    gdouble dk;
    gdouble ehs = 0.;
    GstFFTF64Complex c_fft[MAXLAG / 2 + 1];
    gdouble s;
    for (i = 0; i < 2 * MAXLAG; i++) {
      gdouble fref = ref_power_spectrum[i];
      gdouble ftest = test_power_spectrum[i];
      if (fref == 0. && ftest == 0.)
        d[i] = 0.;
      else
        d[i] = log (ftest / fref);
    }

    do_xcorr(d, c);

    d0 = c[0];
    dk = d0;
#if defined(EHS_SUBTRACT_DC_BEFORE_WINDOW) && EHS_SUBTRACT_DC_BEFORE_WINDOW
    /* in the following, the mean is subtracted before the window is applied as
     * suggested by [Kabal03], although this contradicts [BS1387]; however, the
     * results thus obtained are closer to the reference */
    gdouble cavg = 0;
    for (i = 0; i < MAXLAG; i++) {
      c[i] /= sqrt (d0 * dk);
      cavg += c[i];
      dk += d[i + MAXLAG] * d[i + MAXLAG] - d[i] * d[i];
    }
    cavg /= MAXLAG;
    for (i = 0; i < MAXLAG; i++)
      c[i] = (c[i] - cavg) * correlation_window[i];
#else
    for (i = 0; i < MAXLAG; i++) {
      c[i] *= correlation_window[i] / sqrt(d0 * dk);
      dk += d[i + MAXLAG] * d[i + MAXLAG] - d[i] * d[i];
    }
#endif
    gst_fft_f64_fft (correlation_fft, c, c_fft);
#if !defined(EHS_SUBTRACT_DC_BEFORE_WINDOW) || !EHS_SUBTRACT_DC_BEFORE_WINDOW
    /* subtracting the average is equivalent to setting the DC component to
     * zero */
    c_fft[0].r = 0;
#endif
    s = c_fft[0].r * c_fft[0].r + c_fft[0].i * c_fft[0].i;
    for (i = 1; i < MAXLAG / 2 + 1; i++) {
      gdouble new_s = c_fft[i].r * c_fft[i].r + c_fft[i].i * c_fft[i].i;
      if (new_s > s && new_s > ehs)
        ehs = new_s;
      s = new_s;
    }
    peaq_movaccum_accumulate (mov_accum, chan, 1000. * ehs, 1.);
  }
}

