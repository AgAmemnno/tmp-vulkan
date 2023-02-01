Blender
=======

Blender is the free and open source 3D creation suite.
It supports the entirety of the 3D pipeline-modeling, rigging, animation, simulation, rendering, compositing,
motion tracking and video editing.

.. figure:: https://code.blender.org/wp-content/uploads/2018/12/springrg.jpg
   :scale: 50 %
   :align: center


WIP
-------



BUILD & TEST
-------
| Windows only, some early development, so be careful.
| Blender.exe cannot be executed as it is an unofficial source for developing prototypes.
**Since it is difficult to debug the source with window using gtest, bf_draw_testing.exe is generated for testing.**
**You can debug with the solution.**
**A custom configuration can be found below.**
https://github.com/AgAmemnno/tmp-vulkan/blob/master/build_files/cmake/config/blender_dev_vulkan.cmake

**Please set the directory.**
    **./**
      **\/lib**
        **\/win64_vc15**
    **\/current_directory**


**Build will start.**

| $git clone git@github.com:AgAmemnno/tmp-vulkan.git  
| $cd tmp-vulkan  
| $make update
| $make debug dev_vulkan builddir build-tmp 


-------
License
-------

Blender as a whole is licensed under the GNU General Public License, Version 3.
Individual files may have a different, but compatible license.



