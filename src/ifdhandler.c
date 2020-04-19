/*
 * Copyright (C) 2017 Christoph Muellner
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <ifdhandler.h>
#include <debuglog.h>

#include "reader.h"
#include "kerkey.h"

#ifndef IFDHANDLERv2

RESPONSECODE IFDHCreateChannelByName(DWORD Lun, LPSTR DeviceName)
{
	if (reader_exists(Lun)) {
		Log2(PCSC_LOG_ERROR, "Lun 0x%lx already open!", Lun);
		return IFD_NO_SUCH_DEVICE;
	}

	struct reader *r = create_reader(Lun);
	if (r == NULL) {
		Log1(PCSC_LOG_ERROR, "Could not create reader!");
		return IFD_NO_SUCH_DEVICE;
	}

	int ret = kerkey_open(r, DeviceName);
	if (ret) {
		free_reader(r);
		Log1(PCSC_LOG_ERROR, "Could not open kerkey!");
		return IFD_NO_SUCH_DEVICE;
	}

	return IFD_SUCCESS;
}

RESPONSECODE IFDHControl(DWORD Lun, DWORD dwControlCode, PUCHAR
	TxBuffer, DWORD TxLength, PUCHAR RxBuffer, DWORD RxLength,
	LPDWORD pdwBytesReturned)
{
	(void) Lun;
	(void) dwControlCode;
	(void) TxBuffer;
	(void) TxLength;
	(void) RxBuffer;
	(void) RxLength;
	(void) pdwBytesReturned;
	return SCARD_E_UNSUPPORTED_FEATURE;
}

#else

RESPONSECODE IFDHControl(DWORD Lun, PUCHAR TxBuffer, DWORD TxLength,
	PUCHAR RxBuffer, PDWORD RxLength)
{
	(void) Lun;
	(void) TxBuffer;
	(void) TxLength;
	(void) RxBuffer;
	(void) RxLength;
	return SCARD_E_UNSUPPORTED_FEATURE;
}

#endif /* IFDHANDLERv2 */

RESPONSECODE IFDHCreateChannel(DWORD Lun, DWORD Channel)
{
	(void) Lun;
	(void) Channel;
	/* No support for channel IDs */
	return IFD_NO_SUCH_DEVICE;
}

RESPONSECODE IFDHCloseChannel(DWORD Lun)
{
	struct reader *r = get_reader(Lun);
	if (r == NULL) {
		Log2(PCSC_LOG_ERROR, "Lun 0x%lx not open!", Lun);
		return IFD_NO_SUCH_DEVICE;
	}

	int ret = kerkey_close(r);
	free_reader(r);
	if (ret)
		return IFD_COMMUNICATION_ERROR;

	return IFD_SUCCESS;
}

RESPONSECODE IFDHGetCapabilities(DWORD Lun, DWORD Tag, PDWORD Length,
	PUCHAR Value)
{
	int ret;

	struct reader *r = get_reader(Lun);
	if (r == NULL) {
		Log2(PCSC_LOG_ERROR, "Lun 0x%lx not open!", Lun);
		return IFD_NO_SUCH_DEVICE;
	}

	switch (Tag) {
		case TAG_IFD_ATR:
			ret = kerkey_get_atr(r, Value, (size_t*)Length);
			if (ret)
				return IFD_COMMUNICATION_ERROR;
			break;

		case TAG_IFD_SIMULTANEOUS_ACCESS:
			Value[0] = MAX_KERKEY_DEVICES;
			*Length = 1;
			break;

		case TAG_IFD_THREAD_SAFE:
			Value[0] = 0;
			*Length = 1;
			break;

		case TAG_IFD_SLOTS_NUMBER:
			Value[0] = 1;
			*Length = 1;
			break;

		case TAG_IFD_SLOT_THREAD_SAFE:
			Value[0] = 0;
			*Length = 1;
			break;

		default:
			return IFD_ERROR_TAG;
	}

	return IFD_SUCCESS;
}

RESPONSECODE IFDHSetCapabilities(DWORD Lun, DWORD Tag, DWORD Length, PUCHAR Value)
{
	(void) Lun;
	(void) Tag;
	(void) Length;
	(void) Value;
	return IFD_ERROR_TAG;
}

RESPONSECODE IFDHSetProtocolParameters(DWORD Lun, DWORD Protocol, UCHAR Flags,
	UCHAR PTS1, UCHAR PTS2, UCHAR PTS3)
{
	(void) Lun;
	(void) Protocol;
	(void) Flags;
	(void) PTS1;
	(void) PTS2;
	(void) PTS3;
	return IFD_NOT_SUPPORTED;
}

RESPONSECODE IFDHPowerICC(DWORD Lun, DWORD Action, PUCHAR Atr, PDWORD
	AtrLength)
{
	int ret;
	struct reader *r = get_reader(Lun);
	if (r == NULL) {
		Log2(PCSC_LOG_ERROR, "Lun 0x%lx not open!", Lun);
		return IFD_NO_SUCH_DEVICE;
	}

	if (Action == IFD_POWER_UP) {
		ret = kerkey_power_up(r);
		if (ret)
			return IFD_ERROR_POWER_ACTION;
		ret = kerkey_get_atr(r, Atr, (size_t*)AtrLength);
		if (ret)
			return IFD_COMMUNICATION_ERROR;
	} else if (Action == IFD_POWER_DOWN) {
		ret = kerkey_power_down(r);
		if (ret)
			return IFD_ERROR_POWER_ACTION;
		memset(Atr, 0, *AtrLength);
		*AtrLength = 0;
	} else if (Action == IFD_RESET) {
		ret = kerkey_warm_reset(r);
		if (ret)
			return IFD_ERROR_POWER_ACTION;
		ret = kerkey_get_atr(r, Atr, (size_t*)AtrLength);
		if (ret)
			return IFD_COMMUNICATION_ERROR;
	} else
		return IFD_NOT_SUPPORTED;

	return IFD_SUCCESS;
}

RESPONSECODE IFDHTransmitToICC(DWORD Lun, SCARD_IO_HEADER SendPci,
	PUCHAR TxBuffer, DWORD TxLength, PUCHAR RxBuffer, PDWORD
	RxLength, PSCARD_IO_HEADER RecvPci)
{
	int ret;

	struct reader *r = get_reader(Lun);
	if (r == NULL) {
		Log2(PCSC_LOG_ERROR, "Lun 0x%lx not open!", Lun);
		return IFD_NO_SUCH_DEVICE;
	}

	memcpy(RecvPci, &SendPci, sizeof(SendPci));

	ret = kerkey_xfer(r, TxBuffer, TxLength, RxBuffer, (size_t*)RxLength);
	if (ret)
		return IFD_COMMUNICATION_ERROR;

	return IFD_SUCCESS;
}

RESPONSECODE IFDHICCPresence(DWORD Lun)
{
	struct reader *r = get_reader(Lun);
	if (r == NULL) {
		Log2(PCSC_LOG_ERROR, "Lun 0x%lx not open!", Lun);
		return IFD_NO_SUCH_DEVICE;
	}

	/* A kerkey cannot be removed... */
	return IFD_SUCCESS;
}
