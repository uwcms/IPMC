/*
 * This file is part of the ZYNQ-IPMC Framework.
 *
 * The ZYNQ-IPMC Framework is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * The ZYNQ-IPMC Framework is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with the ZYNQ-IPMC Framework.  If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * List of variables that are automatically assigned during compilation to keep
 * track of software version, git info and other desirable metrics.
 *
 * const ints are static by default and will be optimized away by the compiler.
 * So, this is not const here.  It is in the headers.  The linker won't care.
 *
 * @{
 */
long int GIT_SHORT_INT   = D_GIT_SHORT_INT;
const char *GIT_SHORT    = D_GIT_SHORT;
const char *GIT_LONG     = D_GIT_LONG;
const char *GIT_DESCRIBE = D_GIT_DESCRIBE;
const char *GIT_BRANCH   = D_GIT_BRANCH;
const char *GIT_STATUS   = D_GIT_STATUS;
const char *COMPILE_DATE = D_COMPILE_DATE;
const char *COMPILE_HOST = D_COMPILE_HOST;
const char *BUILD_CONFIGURATION = D_BUILD_CONFIGURATION;
/** @}*/
