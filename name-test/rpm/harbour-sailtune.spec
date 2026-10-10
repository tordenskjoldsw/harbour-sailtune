Name:       harbour-sailtune

Summary:    Music from your own Jellyfin server, also offline
Version:    0.0.1
Release:    1
License:    MIT
URL:        https://github.com/tordenskjoldsw
Source0:    %{name}-%{version}.tar.bz2
Requires:   sailfishsilica-qt5 >= 0.10.9
BuildRequires:  pkgconfig(sailfishapp) >= 1.0.2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  desktop-file-utils

%description
Name check build of SailTune, a music player for a Jellyfin server.

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
