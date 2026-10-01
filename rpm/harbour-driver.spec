Name:       harbour-driver

Summary:    Driver trip planner
Version:    0.8.1
Release:    1
License:    GPLv3
URL:        https://example.org/harbour-driver
Source0:    %{name}-%{version}.tar.bz2
Requires:   sailfishsilica-qt5 >= 0.10.9
Requires:   qt5-qtpositioning
Requires:   qt5-qtdeclarative-import-positioning
Requires:   qt5-qtgraphicaleffects
Requires:   mapboxgl-qml
BuildRequires:  pkgconfig(sailfishapp) >= 1.0.2
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(Qt5Quick)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(Qt5Positioning)
BuildRequires:  pkgconfig(Qt5Test)
BuildRequires:  desktop-file-utils

%description
WeGo-style trip planner for Sailfish OS

%package fixtures
Summary:    Sample HERE API responses for %{name} UI development and screenshots
Requires:   %{name} = %{version}-%{release}

%description fixtures
Local JSON fixtures that let %{name} run its whole UI without a HERE API key,
via the --fixtures command-line flag. Not needed for normal use.

%prep
%setup -q -n %{name}-%{version}

%build

%qmake5

%make_build


%install
%qmake5_install


desktop-file-install --delete-original       \
  --dir %{buildroot}%{_datadir}/applications             \
   %{buildroot}%{_datadir}/applications/*.desktop

%check
%qmake5 tests/tests.pro
%make_build
./tst_driver

%files
%defattr(-,root,root,-)
%{_bindir}/%{name}
%{_datadir}/%{name}
%exclude %{_datadir}/%{name}/fixtures
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/*/apps/%{name}.png

%files fixtures
%defattr(-,root,root,-)
%{_datadir}/%{name}/fixtures
