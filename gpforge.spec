#
# GPFORGE specfile
#
# (C) Cyril Hrubis metan{at}ucw.cz 2026
#

Summary: A bitmap font editor for gfxprim fonts
Name: gpforge
Version: git
Release: 1
License: GPL-2.0-or-later
Group: Productivity/Graphics/Other
Url: https://github.com/gfxprim/gpforge
Source: gpforge-%{version}.tar.bz2
BuildRequires: libgfxprim-devel

BuildRoot: %{_tmppath}/%{name}-%{version}-buildroot

%description
A bitmap font editor for gfxprim fonts, imports and exports BDF and exports
gfxprim C fonts.

%prep
%setup -n gpforge-%{version}

%build
CFLAGS="%{optflags}" make %{?jobs:-j%jobs}

%install
DESTDIR="$RPM_BUILD_ROOT" make install

%files -n gpforge
%defattr(-,root,root)
%{_bindir}/gpforge
%{_bindir}/gpforge-cli
%dir %{_sysconfdir}/gp_apps/
%dir %{_sysconfdir}/gp_apps/gpforge/
%{_sysconfdir}/gp_apps/gpforge/layout.json
%{_datadir}/applications/gpforge.desktop

%changelog
* Sun Sep 27 2026 Cyril Hrubis <metan@ucw.cz>

  Initial version.
