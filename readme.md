********************
* EDK2 SHIM module *
********************

To build you need EDK2 and a supported compiler.
Copy Shim directory edk2 folder and run edksetup.sh or Edk2Setup.bat

Vendor certificate  you can put in vendor_cert.h using bin2c or similar tool.
Vendor DBX goes into vendor_dbx.h using the same tool as the above.

Predefined certificate is present in shim.crt
Key is in shim.key

Default binary to load is the Clover boot manager.
This can be changed to GRUB or ELILO.
