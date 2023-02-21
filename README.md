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
| I drew the first screen with vulkan.
| But there are still many funny things.
-------
https://github.com/AgAmemnno/tmp-vulkan/tree/master/tests/png
-------
| Added windows backtrace functionality to guarded allocator for debugging purposes.
| Since the build takes time, it is executed in the callback function.
-------
https://github.com/AgAmemnno/tmp-vulkan/blob/d3f8a5259cf87a019acc1b16f8654406b0717a59/source/creator/creator.c#L267
-------
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

| blender.exe --debug-gpu --gpu-backend vulkan --debug-value -7777 

-------
License
-------

Blender as a whole is licensed under the GNU General Public License, Version 3.
Individual files may have a different, but compatible license.



