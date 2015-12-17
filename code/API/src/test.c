sirius_init("config.xml")

if (mode == WRITE)
{
sirius_open()
sirius_write("primary var", p)
sirius_write("derived var", d)
sirius_close()
sirius_finalize()
}

if (mode == READ)
{
sirius_read_open()
sirius_schedule_read(fp, sel, varname, "deadline=10mins,location=ssd,duration=1day,err=0.0001", data)
sirius_perform_read()
sirius_read_close()
}

