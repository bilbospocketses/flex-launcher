---
layout: default
title: Download
---
{% assign release = site.data.release %}
## License
Flex Launcher is released under the [GNU General Public License v3.0](https://github.com/bilbospocketses/flex-launcher/blob/master/LICENSE). It started from complexlogic's original Flex Launcher (v2.2), which was released into the public domain under the Unlicense.

## Download
Binary packages are available for Windows 64 bit, Linux x86-64, and Raspberry Pi. The latest stable release is version {{ release.version }}.


<table style="border: none" width="75%">
  <tbody>
    <tr>
      <td class="download" width="170"><img src="/flex-launcher/assets/icons/windows.svg" class="download" style="width:128px"></td>
      <td class="download">
        <h4>Windows 10/11:</h4>
        <ul><li><a href="{{ release.windows.url }}">{{ release.windows.name }}</a> (x64 package)</li>
		</ul>
      </td>
    </tr>
    <tr>
      <td class="download" width="170"><img src="/flex-launcher/assets/icons/linux.svg" class="download" style="width:128px"></td>
      <td class="download">
        <h4>APT-based (Debian, Ubuntu):</h4>
        <ul><li><a href="{{ release.deb_amd64.url }}">{{ release.deb_amd64.name }}</a> (amd64 package)</li>
        </ul>
        <h4>Pacman-based (Arch, Manjaro):</h4>
        <ul><li><a href="{{ release.arch.url }}">{{ release.arch.name }}</a> (x86-64 package)</li>
        </ul>
      </td>
    </tr>
    <tr>
      <td class="download" width="170"><img src="/flex-launcher/assets/icons/raspberry_pi.svg" class="download" style="width:128px"></td>
      <td class="download">
        <h4>Raspberry Pi (64 bit only)</h4>
        <ul><li><a href="{{ release.deb_arm64.url }}">{{ release.deb_arm64.name }}</a> (arm64 package)</li>
		</ul>
      </td>
    </tr>
</tbody>
</table>


## Source Code
If your platform is not listed above, you're interested in development, or you simply prefer compiling your own software, you can download source packages below. See the [compilation guide](compilation) for build instructions.

<table style="border: none" width="75%">
  <tbody>
    <tr>
      <td class="download" width="170"><img src="/flex-launcher/assets/icons/source.svg" class="download" style="width:128px"></td>
      <td class="download">
        <h4>Source Packages:</h4>
        <ul>
          <li><a href="https://github.com/{{ site.repository }}/archive/refs/tags/{{ release.tag }}.zip">Flex Launcher {{ release.tag }} Source (.zip)</a></li>
          <li><a href="https://github.com/{{ site.repository }}/archive/refs/tags/{{ release.tag }}.tar.gz">Flex Launcher {{ release.tag }} Source (.tar.gz)</a></li>
		</ul>
      </td>
    </tr>
  </tbody>
</table>


