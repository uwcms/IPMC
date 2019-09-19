# This file is part of the ZYNQ-IPMC Framework.
#
# The ZYNQ-IPMC Framework is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# The ZYNQ-IPMC Framework is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with the ZYNQ-IPMC Framework.  If not, see <https://www.gnu.org/licenses/>.
        
proc generate {drv_handle} {
    ::hsi::utils::define_include_file $drv_handle "xparameters.h" "mgmt_zone_ctrl" "NUM_INSTANCES" "DEVICE_ID"  "C_S_AXI_BASEADDR" "C_S_AXI_HIGHADDR" "C_MZ_CNT" "C_HF_CNT" "C_PWREN_CNT"
    ::hsi::utils::define_config_file $drv_handle "mgmt_zone_ctrl_g.c" "Mgmt_Zone_Ctrl" "DEVICE_ID"  "C_S_AXI_BASEADDR" "C_MZ_CNT" "C_HF_CNT" "C_PWREN_CNT"
    ::hsi::utils::define_canonical_xpars $drv_handle "xparameters.h" "mgmt_zone_ctrl" "NUM_INSTANCES" "DEVICE_ID"  "C_S_AXI_BASEADDR" "C_S_AXI_HIGHADDR" "C_MZ_CNT" "C_HF_CNT" "C_PWREN_CNT"
}
