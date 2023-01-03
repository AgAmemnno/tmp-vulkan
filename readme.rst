
.. Keep this document short & concise,
   linking to external resources instead of including content in-line.
   See 'release/text/readme.html' for the end user read-me.


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
**Please set the directory.**
    **./**
      **\/lib**
        **\/win64_vc15**
    **\/current_directory**

| $git clone git@github.com:AgAmemnno/tmp-vulkan.git  
| $cd tmp-vulkan  
| $make update
**Since it is difficult to debug the source with window using gtest, draw_testing.exe is generated for testing.**
**A custom configuration can be found below.**
https://github.com/AgAmemnno/tmp-vulkan/blob/master/build_files/cmake/config/blender_dev_vulkan.cmake

**Build will start.**
| 
| $make dev_vulkan builddir build-tmp 


-------
License
-------

Blender as a whole is licensed under the GNU General Public License, Version 3.
Individual files may have a different, but compatible license.



