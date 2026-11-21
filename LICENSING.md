# LILLA PitchShift licensing

The original LILLA PitchShift code is licensed under **GNU GPL version 3 or,
at your option, any later version** (`GPL-3.0-or-later`). Third-party material
retains its own copyright notices and license terms. The complete GPL version 3
text is in [LICENSE](LICENSE).

Copyright (c) Sandro Grassia. Contact: info@lillasampler.it.

LILLA PitchShift is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the Free
Software Foundation, either version 3 of the License, or (at your option)
any later version.

LILLA PitchShift is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along with
LILLA PitchShift. If not, see <https://www.gnu.org/licenses/>.

## Scope and retained notices

This declaration covers original project code, tests, build scripts and
documentation in this revision, including project-authored files without an
individual license header. It also covers original LILLA contributions to
derived components, subject to the original material's license conditions.
It does not replace a third party's license or remove any existing permission.
Earlier distributed revisions retain the permissions under which they were
released.

Third-party material includes the pre-existing portions of the local audio
components, the SOSAndroid-derived FRAM driver, dependencies downloaded by
PlatformIO, the Teensy framework and the compiler runtime libraries. Notices
in a component's header also need to be retained with its corresponding
implementation; a LILLA author comment in a `.cpp` file does not erase the
component's upstream attribution.

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for the component inventory,
versions, upstream locations and copies of their notices. The GPLv2 text kept
in [LICENSES/GPL-2.0.txt](LICENSES/GPL-2.0.txt) documents the SPI library's
alternative license and the previous repository license; it is not the default
license for the original code in this revision.

For dr_libs, this distribution selects the offered **MIT-0** alternative.
The embedded minimp3 code retains its separate public-domain/CC0 notice.
For SPI, the offered **LGPL-2.1** alternative is selected; the notices still
record both upstream alternatives.

## Compatibility of the firmware combination

The firmware uses ARM CMSIS code declared under Apache-2.0. GPLv3 permits this
combination where GPLv2-only does not. This is the reason for selecting GPLv3
for the original project code. See the
[Apache Software Foundation compatibility explanation](https://www.apache.org/licenses/GPL-compatibility.html).

### PJRC condition: evidence and scope

The Teensy core contains a PJRC-specific condition: when its software is
incorporated into a build system offering a list of target devices, similar
PJRC devices must be included and selectable in the same manner. This is a
mandatory condition, distinct from the request to retain copyright notices.
It is not a general requirement that every firmware run only on PJRC hardware.
See the [upstream startup.c notice](https://github.com/PaulStoffregen/cores/blob/master/teensy4/startup.c).

The exact installed notices are retained in
[LICENSES/Teensy-core.txt](LICENSES/Teensy-core.txt), including `startup.c`,
`Print.h` and `AudioStream.h`, and in the affected Audio-library notices.
The installed package is `framework-arduinoteensy@1.162.0`; the manifest
records its inspected source hashes. Upstream `master` is a reference, not
the immutable record of this build.

This concerns code used by the firmware, not just an unused tool: inspection
of `LIlla_PitchShift_v_0_0_13.elf` with `arm-none-eabi-nm -C` confirms the
`ResetHandler` and `ResetHandler2` symbols implemented in `startup.c`.
The current build selects `board = teensy41`. No target-selection violation
has been identified in this configuration. However, selecting that board
alone does not establish compatibility for downstream redistribution and reuse.

### Why GPLv3 compatibility is unresolved

[GPLv3](https://www.gnu.org/licenses/gpl-3.0.en.html), also reproduced in
[LICENSE](LICENSE), requires licensing a covered combined work as a whole
under its terms (section 5(c)). Section 7 allows specified additional terms,
such as preserving notices, but treats other non-permissive terms as further
restrictions; section 10 prohibits imposing further restrictions on recipients.
If applicable obligations cannot be met together, section 12 does not permit
distribution of that combination.

**Our assessment is that the PJRC target-selection obligation presents a
credible additional-restriction concern.** Requiring support for particular
devices does not appear to fit the permitted categories in section 7.
This is an interpretation of the texts, not an authoritative PJRC or FSF ruling
on this license. The scope of incorporation into a build system, and whether
the condition constrains recipients of this particular combined work, remain
material interpretive questions. A firmware HEX is not itself a target-selection
menu, but that observation alone does not settle recipients' reuse rights.

The review on 2026-11-20 did not locate an explicit PJRC clarification or
alternative permission settling GPLv3 distribution of this combination.
That search result is not proof that no such permission exists. Other projects
using Teensy with GPL code are not a substitute for an applicable permission.

Merely retaining the notice documents the condition; it does not resolve a
conflict. Nor does section 7 authorize deleting conditions from arbitrary
third-party licenses: the PJRC grant is not a GPL grant, and its permission
to sublicense is expressly subject to its conditions. This review therefore
does not assume that the target-selection condition can be removed.

### Ways to resolve the question

1. **Obtain applicable permission from the PJRC copyright holders.** Request
   a written clarification of the condition's scope for this firmware and its
   corresponding sources. If the condition survives GPL redistribution,
   request an alternative license or waiver covering the relevant code and
   downstream recipients, not merely a statement that this build uses Teensy.
2. **Consider an explicit linking exception for LILLA.** GPLv3 section 7 and
   the [GNU FAQ on incompatible libraries](https://www.gnu.org/licenses/gpl-faq.en.html#GPLIncompatibleLibs)
   describe permission to combine GPL code with a specified library under
   other terms. The holders of the relevant LILLA copyrights can grant this
   permission for their code. It would preserve PJRC's conditions and change
   LILLA's grant to GPLv3-or-later with an explicit exception. It cannot waive
   another author's GPL requirements. Before adopting it, verify all linked
   copyleft material, including optional SdFat GPL portions, and satisfy the
   separate LGPL source/relinking requirements. File-header authorship is
   useful evidence, but does not alone establish authority over every contribution.
3. **Replace affected code or reconsider the original-code license.** Either
   route requires a fresh audit of the resulting combination. Apache-2.0
   compatibility explains choosing GPLv3 over GPLv2-only; Apache-2.0 does not
   itself require LILLA to adopt GPL. Relicensing original code cannot change
   the terms of third-party material or revoke permissions already granted.

For a distribution intended to remain under GPLv3 without a project-specific
exception, the first route is the most direct. A useful question to the rights
holder is whether condition 2 permits distribution and downstream modification
and redistribution of the combined LILLA/Teensy firmware and its corresponding
sources under GPLv3 without an additional device-selection obligation; if not,
whether they grant an alternative permission for that purpose.

**Current status: a concrete compatibility concern remains unresolved, not a
confirmed infringement by this project.** No linking exception, waiver or
alternative license is granted by this analysis. Obtain an applicable permission
or a reasoned legal resolution before presenting the combined firmware as
cleared for distribution under GPLv3 without an exception.

## Distributing firmware

For a source-and-binary release, distribute the following together:

1. The named HEX and its SHA-256, with the exact source revision that produced it.
2. The corresponding project sources, `platformio.ini`, build scripts and
   instructions, plus the required dependency sources or another delivery
   arrangement that satisfies the applicable GPL/LGPL source obligations.
3. `LICENSE`, `LICENSING.md`, `THIRD_PARTY_NOTICES.md` and the complete `LICENSES/`
   directory. Preserve upstream copyright, attribution, funding and modification
   notices, and any applicable upstream `NOTICE` files.
4. The information required to rebuild and relink the firmware with modified
   LGPL components. If the distribution is in a GPLv3 User Product and section 6
   requires Installation Information, provide that information as well.

A source tag, a hash manifest, or a link to a moving upstream branch is not
by itself a complete corresponding-source delivery. Keep a reproducible copy
of the required inputs for each released binary and choose a source-delivery
method allowed by the actual licenses. Merely adding these files to the
repository does not complete a binary release's source obligations.

The GCC Runtime Library Exception applies only to the runtime material that
grants it; it is not a general exception for LILLA code. Newlib carries several
licenses rather than one uniform license. Their supplied notices are included
in the inventory.

## Updating the inventory

The inventory records the installed inputs inspected on 2026-11-20 for
`lilla_fft_r2`, including Adafruit GFX 1.13.0, which was resolved from the
version constraint in `platformio.ini`. A dependency update or a fresh resolution
may require new notices even when the application source is unchanged.

[LICENSES/manifest.json](LICENSES/manifest.json) records the origins and SHA-256
values of the supplied license texts and notices, and the source files from
which notices were copied. Text hashes normalize CRLF to LF so that Git's line
ending conversion does not invalidate them. The recorded firmware and archive
hashes use unchanged bytes. This manifest is a provenance record, not a source
archive or a certificate of legal compliance.

When updating dependencies, inspect their actual file-level notices, refresh
the inventory and texts, and retain the source package needed to reproduce the
release. Package-level license labels alone may omit exceptions or optional
components with different licenses.
