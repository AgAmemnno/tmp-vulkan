
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
./<br/>
   /lib/win64_vc15<br/>
   /current_directory<br/>
<br/>
$git clone git@github.com:AgAmemnno/tmp-vulkan.git<br/>
$cd tmp-vulkan<br/>
$make update<br/>
<br/>
The following flags should be ON.<br/>
WITH_VULKAN_BACKEND=ON<br/>
WITH_VULKAN_DRAW_TESTS=ON<br/>
WITH_GTESTS=ON<br/>
<br/>
$make<br/>
Build will start.



-------
License
-------

Blender as a whole is licensed under the GNU General Public License, Version 3.
Individual files may have a different, but compatible license.



