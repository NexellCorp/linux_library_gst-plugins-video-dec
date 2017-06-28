Name:    gst-plugins-video-dec
Version: 1.0.3
Release: 1
License: LGPLv2+
Summary: nexell video decoder gstreamer plugin
Group: Development/Libraries
Source:  %{name}-%{version}.tar.gz

BuildRequires:	pkgconfig automake autoconf libtool
BuildRequires:	pkgconfig(glib-2.0)
BuildRequires:	gstreamer1-devel
BuildRequires:	glibc-devel
BuildRequires:	gstreamer1-plugins-base-devel
BuildRequires:	nx-gst-meta-devel
BuildRequires:	nx-video-api-devel

Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig

%description
Nexell video decoder gstreamer plugin

%prep
%setup -q

%build
autoreconf -v --install || exit 1
%configure
make %{?_smp_mflags}

%postun -p /sbin/ldconfig

%install
rm -rf %{buildroot}
make install DESTDIR=%{buildroot}

find %{buildroot} -type f -name "*.la" -delete

%files
%{_libdir}/gstreamer-1.0/libgstnxvideodec.so*
%license LICENSE.LGPLv2+
