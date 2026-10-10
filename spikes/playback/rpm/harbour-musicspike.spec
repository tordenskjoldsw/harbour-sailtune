Name:       harbour-musicspike

Summary:    Spike: music from a Jellyfin server on Sailfish OS
Version:    0.0.4
Release:    1
License:    MIT
Source0:    %{name}-%{version}.tar.bz2
Requires:   sailfishsilica-qt5 >= 0.10.9
BuildRequires:  pkgconfig(sailfishapp) >= 1.0.2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  desktop-file-utils

%description
Measures whether a Harbour app can stream and download music from a
Jellyfin server, play it with the screen off and be controlled from the
lock screen.

%prep
%setup -q -n %{name}-%{version}

%build

%qmake5 VERSION=%{version}

%make_build

%install
%qmake5_install

desktop-file-install --delete-original       \
  --dir %{buildroot}%{_datadir}/applications             \
   %{buildroot}%{_datadir}/applications/*.desktop

%files
%{_bindir}/%{name}
%{_datadir}/%{name}
%{_datadir}/applications/%{name}.desktop
