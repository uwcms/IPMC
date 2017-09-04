# IPMC
University of Wisconsin IPMC

## Instructions for a clean checkout and SDK:
1. git clone git@github.com:uwcms/IPMC.git
2. Open the Vivado 2017.2 project (vivado Vivado/ipmc_zynq_vivado.xpr)
3. (optional) Generate Bitstream
4. (if bitstream generated) File -> Export -> Export Hardware (include bitstream checked)
5. File -> Launch SDK (accept defaults, click OK)
6. In SDK, File -> Import -> General -> Existing Projects into Workspace -> Browse (should be the IPMC .sdk folder by default). Click Finish.

Compilation of all projects should start automatically.
In case of issue, regenerate BSP files by right click on ipmc_standalone_bsp -> Re-generate BSP Sources.
