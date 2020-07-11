Name:		calendar
Version:	0.1.0
Release:	1%{?dist}
Summary:	UNIX calendar(1) reminder service with Chinese calendar support

License:	BSD
URL:		https://github.com/liweitianux/ccalendar

BuildRequires:	make, gcc

%description
The "calendar" utility provides a reminder service by checking a calendar file
and showing events within the specified date range.  This "calendar" utility
has Chinese calendar support implemented with astronomical algorithms.

%prep
# empty

%build
export CFLAGS="$RPM_OPT_FLAGS"
make PREFIX=%{_prefix} ETC_DIR=%{_sysconfdir} MAN_DIR=%{_mandir} \
	CALENDAR_DIR=%{_datadir}/%{name}

%install
rm -rf %{buildroot}
make install PREFIX=%{buildroot}%{_prefix} \
	ETC_DIR=%{buildroot}%{_sysconfdir} \
	MAN_DIR=%{buildroot}%{_mandir} \
	CALENDAR_DIR=%{buildroot}%{_datadir}/%{name}

%files
%{_bindir}/%{name}
%{_sysconfdir}/%{name}/*
%dir %{_sysconfdir}/%{name}
%config(noreplace) %{_sysconfdir}/%{name}/default
%{_mandir}/man1/%{name}.1.gz
%dir %{_mandir}/man1
%{_datadir}/%{name}/*
%dir %{_datadir}/%{name}
%doc README.md
%license LICENSE

%changelog
* Sat May 09 2020 Aaron LI <aly@aaronly.me> - 0.1.0-1
- Initial package for version 0.1.0
