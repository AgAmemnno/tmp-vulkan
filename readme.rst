
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


WIP ( forked from tmp-vulkan  args [ --gpu-backend vulkan] WITH_VULKAN_BACKEND=ON WITH_GTESTS=ON) 

 cmake  ../blender -G "Visual Studio 16 2019" -A x64  -DCMAKE_CONFIGURATION_TYPES:STRING="Debug" -DCMAKE_INSTALL_PREFIX:PATH="install/dir" -DWITH_BUILDINFO=OFF -DWITH_VULKAN_BACKEND=ON -DWITH_GTESTS=ON -DWITH_CYCLES_OSL=OFF
cmake --build . 
cmake --install . --prefix "/home/myuser/installdir"
bf_gpu_test.exe 

-------

License
-------

Blender as a whole is licensed under the GNU General Public License, Version 3.
Individual files may have a different, but compatible license.


