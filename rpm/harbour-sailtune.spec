Name:       harbour-sailtune

Summary:    Music player for the phone's music and your own server
Version:    0.5.1
Release:    1
License:    MIT
URL:        https://github.com/tordenskjoldsw/harbour-sailtune
Source0:    %{name}-%{version}.tar.bz2
Requires:   sailfishsilica-qt5 >= 0.10.9
BuildRequires:  pkgconfig(sailfishapp) >= 1.0.2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(Qt5Sql)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(Qt5Concurrent)
BuildRequires:  pkgconfig(Qt5Multimedia)
BuildRequires:  desktop-file-utils

%description
SailTune plays the music on the phone and the music on your own server:
streamed while the server is reachable, or downloaded to play without a
network.

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
%{_datadir}/icons/hicolor/*/apps/%{name}.png
