Name:       harbour-sourcespike

Summary:    Spike: the phone's music index and Jellyfin server discovery
Version:    0.0.1
Release:    1
License:    MIT
Source0:    %{name}-%{version}.tar.bz2
Requires:   sailfishsilica-qt5 >= 0.10.9
BuildRequires:  pkgconfig(sailfishapp) >= 1.0.2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(Qt5Concurrent)
BuildRequires:  desktop-file-utils

%description
Measures whether a Harbour app can read the phone's music from the
system's media index over D-Bus, find the album art the system extracted,
play a local file, and find Jellyfin servers in the home network.

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
