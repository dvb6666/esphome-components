#include "nartis-100.h"

namespace esphome {
namespace nartis100 {

static const char *const TAG = "nartis100";

#define CHUNK_SIZE 34

#define PHASE_LENGTH 10
#define skip_next_phases(er) { this->phase_ += (PHASE_LENGTH - phase); this->error_ = er; return; }
#define return_to_phase(ph) { this->phase_ += (ph - phase); return; }

static const uint8_t app_const_name[] = {0xa1, 0x09, 0x06, 0x07, 0x60, 0x85, 0x74, 0x05, 0x08, 0x01, 0x01};
static const uint8_t asce[] = {0x8a, 0x02, 0x07, 0x80};
static const uint8_t mech_name[] = {0x8b, 0x07, 0x60, 0x85, 0x74, 0x05, 0x08, 0x02, 0x01};
static const uint8_t user_info[] = {0xbe, 0x10, 0x04, 0x0e, 0x01, 0x00, 0x00, 0x00, 0x06, 0x5f, 0x1f, 0x04, 0x00, 0x00, 0x1e, 0x9d, 0xff, 0xff};

static const uint16_t fcstab[256] = {
     0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
     0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
     0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
     0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
     0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
     0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
     0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
     0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
     0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
     0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
     0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
     0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
     0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
     0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
     0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
     0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
     0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
     0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
     0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
     0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
     0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
     0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
     0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
     0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
     0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
     0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
     0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
     0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
     0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
     0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
     0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
     0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};


int Command::fill_notification_request(package_t *raw_package) {
  uint8_t *pkt_buff = (uint8_t*)raw_package;
  set_header(raw_package);

  // I don't know how true
  raw_package->header.control = ((((meter.rrr << 5) + (meter.sss << 1)) | 0x10)) & 0xFE;
  // raw_package->header.control = ((((meter.rrr << 5) + (meter.sss << 1)) | 0x10) + 2) & 0xFE;
  meter.format.length += 3;                       /* + size command + size HCS   */

  uint8_t *format = (uint8_t*)&(meter.format);
  raw_package->header.format[0] = format[1];
  raw_package->header.format[1] = format[0];
  uint16_t crc = checksum(pkt_buff + 1, meter.format.length - 2);
  raw_package->data[0] = crc & 0xff;
  raw_package->data[1] = (crc >> 8) & 0xff;
  raw_package->data[2] = FLAG;
  return meter.format.length + 2;
}

bool Command::publish_result() {
  if (publish_size_ > 0 && publish_counter_ < publish_size_) {
    // ESP_LOGV(TAG, "Publish %d", publish_counter_);
    on_publish_(publish_counter_++);
    if (publish_counter_ < publish_size_) {
      return false;
    } else {
      publish_counter_ = 0;
    }
  }
  return true;
}

size_t Command::set_header(package_t *raw_package) {
  raw_package->header.flag = FLAG;
  meter.format.length = 2;   /* format 2 bytes */
  set_address(raw_package->header.addr, 2, meter.server_lower_addr, meter.server_upper_addr);
  set_address(raw_package->header.addr + 2, 1, 0, meter.client_addr);
  meter.format.length += 3;
  return meter.format.length;
}

uint8_t Command::set_address(uint8_t *buff, uint8_t len, uint16_t lower, uint16_t upper) {
    uint8_t ret = true;
    switch(len) {
        case 1:
            *buff = ((uint8_t)(upper << 1)) | 0x01;
            break;
        case 2:
            *buff =     ((uint8_t)(upper << 1)) & 0x7E;
            *(buff+1) = ((uint8_t)(lower << 1)) | 0x01;
            break;
        case 4:
            *buff =     ((uint8_t)(upper >> 7)) & 0x7E;
            *(buff+1) = ((uint8_t)(upper << 1)) & 0x7E;
            *(buff+2) = ((uint8_t)(lower >> 7)) & 0x7E;
            *(buff+3) = ((uint8_t)(lower << 1)) | 0x01;
            break;
        default:
            ret = false;
            break;
    }
    return ret;
}

uint8_t Command::get_address(uint8_t *buff, uint8_t len, uint16_t *lower, uint16_t *upper) {
    uint8_t ret = true;
    *lower = 0;
    *upper = 0;
    switch(len) {
        case 1:
            *upper = (uint16_t)((*buff >> 1) & 0x7f);
            break;
        case 2:
            *upper = (uint16_t)((*buff >> 1) & 0x7f);
            *lower = (uint16_t)((*(buff+1) >> 1) & 0x7f);
            break;
        case 4:
            *lower = (uint16_t)((*buff >> 1) & 0x7f);
            *lower = *lower << 8;
            *lower = *lower + ((*(buff+1) >> 1) & 0x7f);
            *upper = (uint16_t)((*(buff+2) >> 1) & 0x7f);
            *upper = *upper << 8;
            *upper = *upper + ((*(buff+3) >> 1) & 0x7f);
            break;
        default:
            ret = false;
            break;
    }
    return ret;
}

uint8_t Command::get_address_size(uint8_t *buff) {
    uint8_t size = 0;
    if (*(buff++) & 1) {
        size++;
    } else {
        size++;
        if (*(buff++) & 1) {
            size++;
        } else {
            size++;
            if (*(buff++) & 1) {
                size++;
            } else {
                size++;
                if (*(buff++) & 1) {
                    size++;
                } else {
                    size = 0;
                }
            }
        }
    }
    if (size != 0 && size != 1 && size != 2 && size != 4) size = 0;
    return size;
}

uint16_t Command::checksum(const uint8_t *src_buffer, size_t len) {
    uint16_t crc = 0xffff;
    while(len--) {
        crc = (crc >> 8) ^ fcstab[(crc ^ *src_buffer++) & 0xff];
    }
    crc ^= 0xffff;
    return crc;
}

int Command::request_data(request_t *request, package_t *raw_package) {
  uint8_t *pkt_buff = (uint8_t*)raw_package;
  uint8_t *info_field_data = (uint8_t*) raw_package->data + 2;
  uint8_t info_field_len = 0;

  info_field_data[info_field_len++] = LSAP;
  info_field_data[info_field_len++] = CMD_LSAP;
  info_field_data[info_field_len++] = 0;
  info_field_data[info_field_len++] = GET_REQUEST;
  info_field_data[info_field_len++] = 0x01;
  info_field_data[info_field_len++] = 0xc1;

  memcpy(info_field_data + info_field_len, request, sizeof(request_t));
  info_field_len += sizeof(request_t);

  uint8_t hcs_len = set_header(raw_package) + 1;
  raw_package->header.control = ((((meter.rrr << 5) + (meter.sss << 1)) | 0x10) + (meter.format.segmentation?0:2)) & 0xFE;
  meter.format.length += 3;                       /* + size command + size HCS   */

  meter.format.length += info_field_len + 2;      /* + size FCS                   */

  uint8_t *format = (uint8_t*)&(meter.format);
  raw_package->header.format[0] = format[1];
  raw_package->header.format[1] = format[0];

  uint16_t crc = checksum(pkt_buff + 1, hcs_len);
  raw_package->data[1] = (crc >> 8) & 0xff;
  raw_package->data[0] = crc & 0xff;

  crc = checksum(pkt_buff+1, meter.format.length - 2);
  raw_package->data[info_field_len + 2] = crc & 0xff;
  raw_package->data[info_field_len + 3] = (crc >> 8) & 0xff;
  raw_package->data[info_field_len + 4] = FLAG;
  return meter.format.length + 2;
}

uint32_t Command::reverse32(uint32_t in) {
    uint32_t out;
    uint8_t *source = (uint8_t*)&in;
    uint8_t *destination = (uint8_t*)&out;
    destination[3] = source[0];
    destination[2] = source[1];
    destination[1] = source[2];
    destination[0] = source[3];
    return out;
}

int CommandSNRM::fill_request(package_t *raw_package) {
  uint8_t *pkt_buff = (uint8_t*)raw_package;
  uint8_t *info_field_data = (uint8_t*) raw_package->data + 2;
  uint8_t info_field_len = 0;

  info_field_data[info_field_len++] = 0x81;
  info_field_data[info_field_len++] = 0x80;
  info_field_data[info_field_len++] = 0x00;
  info_field_data[info_field_len++] = 0x05;
  info_field_data[info_field_len++] = 0x02;
  info_field_data[info_field_len++] = (meter.max_info_field_tx >> 8) & 0xff;
  info_field_data[info_field_len++] = meter.max_info_field_tx & 0xff;
  info_field_data[info_field_len++] = 0x06;
  info_field_data[info_field_len++] = 0x02;
  info_field_data[info_field_len++] = (meter.max_info_field_rx >> 8) & 0xff;
  info_field_data[info_field_len++] = meter.max_info_field_rx & 0xff;
  info_field_data[info_field_len++] = 0x07;
  info_field_data[info_field_len++] = 0x04;
  info_field_data[info_field_len++] = (meter.window_tx >> 24) & 0xff;
  info_field_data[info_field_len++] = (meter.window_tx >> 16) & 0xff;
  info_field_data[info_field_len++] = (meter.window_tx >> 8) & 0xff;
  info_field_data[info_field_len++] = meter.window_tx & 0xff;
  info_field_data[info_field_len++] = 0x08;
  info_field_data[info_field_len++] = 0x04;
  info_field_data[info_field_len++] = (meter.window_rx >> 24) & 0xff;
  info_field_data[info_field_len++] = (meter.window_rx >> 16) & 0xff;
  info_field_data[info_field_len++] = (meter.window_rx >> 8) & 0xff;
  info_field_data[info_field_len++] = meter.window_rx & 0xff;
  info_field_data[2] = info_field_len-3;

  uint8_t hcs_len = set_header(raw_package) + 1;
  raw_package->header.control = SNRM;
  meter.format.length += 3;                       /* + size command + size HCS   */
  meter.format.length += info_field_len + 2;      /* + size FCS                   */

  uint8_t *format = (uint8_t*)&(meter.format);
  raw_package->header.format[0] = format[1];
  raw_package->header.format[1] = format[0];

  uint16_t crc = checksum(pkt_buff + 1, hcs_len);
  raw_package->data[1] = (crc >> 8) & 0xff;
  raw_package->data[0] = crc & 0xff;

  crc = checksum(pkt_buff + 1, meter.format.length - 2);
  raw_package->data[info_field_len + 2] = crc & 0xff;
  raw_package->data[info_field_len + 3] = (crc >> 8) & 0xff;
  raw_package->data[info_field_len + 4] = FLAG;

  return meter.format.length + 2;
}

int CommandOpenSession::fill_request(package_t *raw_package) {
  uint8_t *pkt_buff = (uint8_t*)raw_package;
  uint8_t *info_field_data = (uint8_t*) raw_package->data + 2;
  uint8_t info_field_len = 0;
  uint8_t aarq_len = 0;
  uint8_t aarq_len_idx;
  uint8_t auth_len = 0;
  uint8_t auth_len_idx;

  info_field_data[info_field_len++] = LSAP;
  info_field_data[info_field_len++] = CMD_LSAP;
  info_field_data[info_field_len++] = 0;
  info_field_data[info_field_len++] = AARQ;
  aarq_len_idx = info_field_len;
  info_field_data[info_field_len++] = aarq_len;
  memcpy(info_field_data + info_field_len, app_const_name, sizeof(app_const_name));
  info_field_len += sizeof(app_const_name);
  aarq_len += sizeof(app_const_name);
  memcpy(info_field_data + info_field_len, asce, sizeof(asce));
  info_field_len += sizeof(asce);
  aarq_len += sizeof(asce);
  memcpy(info_field_data + info_field_len, mech_name, sizeof(mech_name));
  info_field_len += sizeof(mech_name);
  aarq_len += sizeof(mech_name);
  info_field_data[info_field_len++] = AUTH;
  aarq_len++;
  auth_len_idx = info_field_len;
  info_field_data[info_field_len++] = auth_len;
  aarq_len++;
  info_field_data[info_field_len++] = 0x80;
  aarq_len++;
  auth_len++;
  info_field_data[info_field_len++] = 0x03;
  aarq_len++;
  auth_len++;
  info_field_data[info_field_len++] = meter.password[0];
  aarq_len++;
  auth_len++;
  info_field_data[info_field_len++] = meter.password[1];
  aarq_len++;
  auth_len++;
  info_field_data[info_field_len++] = meter.password[2];
  aarq_len++;
  auth_len++;
  info_field_data[auth_len_idx] = auth_len;
  memcpy(info_field_data + info_field_len, user_info, sizeof(user_info));
  info_field_len += sizeof(user_info);
  aarq_len += sizeof(user_info);
  info_field_data[aarq_len_idx] = aarq_len;

  uint8_t hcs_len = set_header(raw_package) + 1;
  raw_package->header.control = 0x10; //(((meter.rrr << 5) + (meter.sss << 1)) | 0x10) & 0xFE;
  meter.format.length += 3;                       /* + size command + size HCS   */

  meter.format.length += info_field_len + 2;      /* + size FCS                   */

  uint8_t *format = (uint8_t*)&(meter.format);

  raw_package->header.format[0] = format[1];
  raw_package->header.format[1] = format[0];

  uint16_t crc = checksum(pkt_buff + 1, hcs_len);

  raw_package->data[1] = (crc >> 8) & 0xff;
  raw_package->data[0] = crc & 0xff;

  crc = checksum(pkt_buff + 1, meter.format.length - 2);
  raw_package->data[info_field_len + 2] = crc & 0xff;
  raw_package->data[info_field_len + 3] = (crc >> 8) & 0xff;
  raw_package->data[info_field_len + 4] = FLAG;

  return meter.format.length + 2;
}

bool CommandOpenSession::process_result(header_t *header, result_package_t *package) {
  uint8_t *ptr = package->buff;
  uint8_t aare_len;
  if ((*ptr++ == LSAP) && (*ptr++ == RESP_LSAP) && *ptr++ == 0 && (*ptr++ == AARE) && (aare_len = *ptr) > 0)
    for(uint8_t i = 0; i < aare_len; i++)
      if (*ptr++ == 0xA2) {
        ptr += *ptr;
        if (*ptr == 0) /* open session successful */
          return true;
      }
  return false;
}

int CommandDisconnect::fill_request(package_t *raw_package) {
  uint8_t *pkt_buff = (uint8_t*)raw_package;
  uint8_t *info_field_data = (uint8_t*) raw_package->data + 2;
  uint8_t info_field_len = 0;

  set_header(raw_package);
  raw_package->header.control = DISC;
  meter.format.length += 3;                       /* + size command + size FCS   */

  uint8_t *format = (uint8_t*)&(meter.format);
  raw_package->header.format[0] = format[1];
  raw_package->header.format[1] = format[0];

  uint16_t crc = checksum(pkt_buff + 1, meter.format.length - 2);
  raw_package->data[0] = crc & 0xff;
  raw_package->data[1] = (crc >> 8) & 0xff;
  raw_package->data[2] = FLAG;
  return meter.format.length + 2;
}


bool CommandGetSerialNumber::process_result(header_t *header, result_package_t *package) {
  uint8_t *ptr = package->buff;
  type_digit_t *unsigned32;
  if ((*ptr++ == LSAP) && (*ptr++ == RESP_LSAP) && *ptr++ == 0 && *ptr == GET_RESPONSE && (unsigned32 = (type_digit_t*)(ptr + 4))->type == TYPE_UNSIGNED_32) {
    number = reverse32(unsigned32->value);
    return true;
  }
  return false;
}

bool CommandGetReleaseDate::process_result(header_t *header, result_package_t *package) {
  uint8_t *ptr = package->buff;
  type_octet_string_t *o_str;
  char tmp[32];
  if ((*ptr++ == LSAP) && (*ptr++ == RESP_LSAP) && *ptr++ == 0 && *ptr == GET_RESPONSE && (o_str = (type_octet_string_t*)(ptr + 4))->type == TYPE_OCTET_STRING && o_str->size > 0) {
    uint8_t *ptr = &o_str->str;
    snprintf(tmp, sizeof(tmp), "%d.%02d.%02d", (uint16_t)((*ptr++) << 8) + *ptr++, *ptr++, *ptr++);
    date = std::string(tmp);
    return true;
  }
  return false;
}

bool CommandGetListData::process_result(header_t *header, result_package_t *package) {
  uint8_t *ptr = package->buff;
  type_octet_string_t *present_date = (type_octet_string_t*)ptr;
  // char date[32];
  if ((*ptr++ == LSAP) && (*ptr++ == RESP_LSAP) && *ptr++ == 0 && *ptr == GET_RESPONSE && (present_date = (type_octet_string_t*)(ptr + 8))->type == TYPE_OCTET_STRING && present_date->size > 0) {
    ptr = &present_date->str;
    // snprintf(date, sizeof(date), "%d.%02d.%02d", (uint16_t)((*ptr++) << 8) + *ptr++, *ptr++, *ptr++);
    // ESP_LOGD(TAG, "Present date: %s", date);
    type_digit_t *p_list = (type_digit_t*)((uint8_t*)&present_date->str + present_date->size);
    // tariffs energy
    type_digit_t *tariff_A_p = p_list + 1;
    for (uint8_t i = 0; i < MAX_TARIFF_COUNT; i++) {
      type_digit_t *tariff_A_m = tariff_A_p + 9;
      if (tariff_A_p->type != TYPE_UNSIGNED_32 || tariff_A_m->type != TYPE_UNSIGNED_32)
        return false;
      energy[i] = (float)(reverse32(tariff_A_p->value) + reverse32(tariff_A_m->value))/1000.0;
      tariff_A_p++;
    }
    // current, voltage, power
    p_list += 39;
    for (uint8_t i = 0; i < 3; i++) {
      if (p_list->type != (i == 0 ? TYPE_SIGNED_32 : TYPE_UNSIGNED_32))
        return false;
      cvp[i] = (float)reverse32(p_list->value) / 1000.0;
      p_list += (i == 0 ? 3 : 2);
    }
    return true;
  }
  return false;
}


Nartis100::Nartis100(uart::UARTComponent *uart, const std::string &password) : uart::UARTDevice(uart) {
  rx_buffer_ = (uint8_t*)&rx_package_;
  tx_buffer_ = (uint8_t*)&tx_package_;

  memset(&meter, 0, sizeof(meter_t));
  meter.client_addr = CLIENT_ADDRESS;
  meter.server_upper_addr = LOGICAL_DEVICE;
  meter.server_lower_addr = PHY_DEVICE;
  meter.max_info_field_rx = MAX_INFO_FIELD;
  meter.max_info_field_tx = MAX_INFO_FIELD;
  meter.window_rx = 1;
  meter.window_tx = 1;
  meter.format.type = TYPE3;
  memcpy(&meter.password, password.c_str(), std::min(sizeof(meter.password), strlen(password.c_str())));
  for (uint8_t i = 0; i < MAX_TARIFF_COUNT; i++)
    this->sensor_energy_[i] = nullptr;
}

void Nartis100::setup() {
  this->phase_ = 0;
  if (this->dir_pin_) {
    this->dir_pin_->setup();
    this->dir_pin_->digital_write(false);
  }
  // read old unknown/unused data
  /*while (this->available())
    this->read();*/

  this->commands_.push_back(new CommandSNRM());
  this->commands_.push_back(new CommandOpenSession());
  if (this->sensor_serial_number_) this->commands_.push_back(new CommandGetSerialNumber([this](const std::string &number) { this->sensor_serial_number_->publish_state(number); }));
  if (this->sensor_release_date_) this->commands_.push_back(new CommandGetReleaseDate([this](const std::string &date) { this->sensor_release_date_->publish_state(date); }));
  this->commands_.push_back(new CommandGetListData([this](uint8_t counter, float c, float v, float p, float t1, float t2, float t3, float t4) {
    if (counter == 0 && this->sensor_current_) this->sensor_current_->publish_state(c);
    if (counter == 1 && this->sensor_voltage_) this->sensor_voltage_->publish_state(v);
    if (counter == 2 && this->sensor_power_) this->sensor_power_->publish_state(p);
    if (counter == 3 && this->sensor_energy_[0]) this->sensor_energy_[0]->publish_state(t1);
    if (counter == 4 && this->sensor_energy_[1]) this->sensor_energy_[1]->publish_state(t2);
    if (counter == 5 && this->sensor_energy_[2]) this->sensor_energy_[2]->publish_state(t3);
    if (counter == 6 && this->sensor_energy_[3]) this->sensor_energy_[3]->publish_state(t4);
  } ));
  this->commands_.push_back(new CommandDisconnect());

  // startup delay
  delay(this->startup_delay_);
}

void Nartis100::dump_config() {
  ESP_LOGCONFIG(TAG, "Nartis-100");
  if (this->dir_pin_) {
    LOG_PIN("  Direction Pin: ", this->dir_pin_);
  } else {
    ESP_LOGCONFIG(TAG, "  Direction Pin: no");
  }
  if (this->sensor_serial_number_)
    LOG_TEXT_SENSOR("  ", "Serial Number Sensor", this->sensor_serial_number_);
  if (this->sensor_release_date_)
    LOG_TEXT_SENSOR("  ", "Release Date Sensor", this->sensor_release_date_);
  for (uint8_t i = 0; i < MAX_TARIFF_COUNT; i++)
    if (this->sensor_energy_[i])
      LOG_SENSOR("  ", "Energy Sensor", this->sensor_energy_[i]);
  if (this->sensor_voltage_)
    LOG_SENSOR("  ", "Voltage Sensor", this->sensor_voltage_);
  if (this->sensor_current_)
    LOG_SENSOR("  ", "Current Sensor", this->sensor_current_);
  if (this->sensor_power_)
    LOG_SENSOR("  ", "Power Sensor", this->sensor_power_);
  if (this->sensor_error_)
    LOG_BINARY_SENSOR("  ", "Error Sensor", this->sensor_error_);
  ESP_LOGCONFIG(TAG, "  Startup Delay: %d", this->startup_delay_);
  LOG_UPDATE_INTERVAL(this);
}

void Nartis100::loop() {
  if (this->phase_ == 0 || this->sleep_time_ > millis())
    return;

  uint32_t phase = this->phase_ % PHASE_LENGTH;
  uint32_t cmd_idx = this->phase_ / PHASE_LENGTH;
  if (cmd_idx >= this->commands_.size() || this->error_) {
    this->phase_ = 0;
    if (!this->error_) this->started_ = true;
    if (this->sensor_error_) this->sensor_error_->publish_state(this->error_);
    ESP_LOGV(TAG, "All phases done");
    return;
  }
  ESP_LOGV(TAG, "Phase %d cmd [%s]", phase, this->commands_[cmd_idx]->get_name().c_str());

  if (!this->commands_[cmd_idx]->is_on_start() || !this->started_) switch (phase) {

  case 1: { // preparing command data
    memset(&this->tx_package_, 0, sizeof(this->tx_package_));
    memset(&this->result_package_, 0, sizeof(this->result_package_));
    this->tx_bytes_length_ = this->commands_[cmd_idx]->fill_request(&this->tx_package_);
  } break;

  case 2: { // preparing to send data
    // read old unknown/unused data
    uint16_t unused;
    for (unused = 0; unused < CHUNK_SIZE && this->available(); unused++)
      this->read();
    if (unused > 0) {
      ESP_LOGV(TAG, "Received %d unknown/unused bytes for command [%s]", unused, this->commands_[cmd_idx]->get_name().c_str());
      return_to_phase(2);
    }
    this->tx_bytes_sent_ = this->rx_bytes_received_ = 0;
    this->rx_bytes_needed_ = this->commands_[cmd_idx]->has_response() ? MIN_FRAME_SIZE : 0;
    this->wait_time_ = millis() + 1000; // timeout 1000 ms
    ESP_LOGV(TAG, "Need to send %d bytes for command [%s]", this->tx_bytes_length_, this->commands_[cmd_idx]->get_name().c_str());
  } break;

  case 3: { // set dir_pin to state SEND
    if (this->dir_pin_) {
      ESP_LOGV(TAG, "Set dir_pin to state SEND");
      this->dir_pin_->digital_write(true);
      delay(5);
    }
  } break;

  case 4: { // sending command data
    uint16_t bytes_to_send = std::min(this->tx_bytes_length_ - this->tx_bytes_sent_, CHUNK_SIZE);
    ESP_LOGV(TAG, "Sending %d of %d bytes for command [%s]", bytes_to_send, this->tx_bytes_length_, this->commands_[cmd_idx]->get_name().c_str());
    this->write_array(this->tx_buffer_ + this->tx_bytes_sent_, bytes_to_send);
    this->flush();
    this->tx_bytes_sent_ += bytes_to_send;
    // exit without increasing phase when not all data was sent
    if (this->tx_bytes_sent_ < this->tx_bytes_length_) {
      return;
    }
    // all data sent
    delay(5);
  } break;

  case 5: { // delay after command sent
    if (this->dir_pin_) {
      this->dir_pin_->digital_write(false);
      ESP_LOGV(TAG, "Set dir_pin to state RECV");
    }
    delay(5);
  } break;

  case 6: { // receiving packet
    for (uint16_t i = 0; i < CHUNK_SIZE && this->rx_bytes_received_ < this->rx_bytes_needed_ && this->available(); i++) {
      uint8_t c = this->read();
      if (this->rx_bytes_received_ == 0 && c != FLAG)
        continue;
      this->rx_buffer_[this->rx_bytes_received_++] = c;
      if (this->rx_bytes_received_ == 3) {
        uint8_t *ptr_format = (uint8_t*)&format;
        *(ptr_format + 1) = this->rx_buffer_[1];
        *ptr_format = this->rx_buffer_[2];
        uint16_t full_size = format.length + (format.segmentation ? 1 : 2);
        ESP_LOGV(TAG, "Received header: type=0x%02X, segmentation=%s, length=%d (full_size=%d)", format.type, format.segmentation ? "yes" : "no", format.length, full_size);
        if (format.type != TYPE3) {
          ESP_LOGV(TAG, "Bad header type 0x%02X received for command [%s]. Reset rx_bytes_received counter", format.type, this->commands_[cmd_idx]->get_name().c_str());
          this->rx_bytes_received_ = 0;
          return_to_phase(6);
        }
        this->rx_bytes_needed_ = full_size;
        if (this->rx_bytes_needed_ < MIN_FRAME_SIZE) {
          ESP_LOGW(TAG, "Too small frame size (%d bytes) received for command [%s]", this->rx_bytes_needed_, this->commands_[cmd_idx]->get_name().c_str());
          skip_next_phases(true);
        } else if (this->rx_bytes_needed_ > sizeof(package_t)) {
          ESP_LOGW(TAG, "Too big frame size (%d bytes) received for command [%s]", this->rx_bytes_needed_, this->commands_[cmd_idx]->get_name().c_str());
          skip_next_phases(true);
        }
      }
    }
    if (this->rx_bytes_received_ < this->rx_bytes_needed_) {
      if (this->wait_time_ < millis()) {
        ESP_LOGD(TAG, "Timed out command [%s]", this->commands_[cmd_idx]->get_name().c_str());
        skip_next_phases(true); // skip next phases (validating, processing, publishing)
      } else {
        ESP_LOGV(TAG, "Waiting next %d bytes", this->rx_bytes_needed_ - this->rx_bytes_received_);
        return;
      }
    } else if (!this->commands_[cmd_idx]->has_response()) {
      ESP_LOGV(TAG, "Skip all next phases for command [%s] without response", this->commands_[cmd_idx]->get_name().c_str());
      skip_next_phases(false);
    }
  } break;

  case 7: { // validating packet
    uint8_t size_d, size_s;
    uint16_t crc, check_crc, lower, upper, data_size;
    if (!format.segmentation && this->rx_buffer_[this->rx_bytes_needed_ - 1] != FLAG) {
      ESP_LOGW(TAG, "Received incomplete packet for command [%s]", this->commands_[cmd_idx]->get_name().c_str());
    } else if ((size_d = Command::get_address_size(this->rx_package_.header.addr)) == 0 || !Command::get_address(this->rx_package_.header.addr, size_d, &lower, &upper)) {
      ESP_LOGW(TAG, "Received packet with bad dest address for command [%s]", this->commands_[cmd_idx]->get_name().c_str());
      skip_next_phases(true);
    } else if ((size_s = Command::get_address_size(this->rx_package_.header.addr + size_d)) == 0 || !Command::get_address(this->rx_package_.header.addr + size_d, size_s, &lower, &upper)) {
      ESP_LOGW(TAG, "Received packet with bad src address for command [%s]", this->commands_[cmd_idx]->get_name().c_str());
      skip_next_phases(true);
    }
    crc = Command::checksum(this->rx_buffer_ + 1, format.length - 2);
    check_crc = this->rx_buffer_[this->rx_bytes_needed_ - (format.segmentation ? 1 : 2)];
    check_crc = (check_crc << 8) + this->rx_buffer_[this->rx_bytes_needed_ - (format.segmentation ? 2 : 3)];
    data_size = this->rx_bytes_needed_ - sizeof(header_t) - (format.segmentation ? 4 : 3);
    if (crc != check_crc) {
      ESP_LOGW(TAG, "Received packet with wrong checksum (0x%04X instead of 0x%04X) for command [%s]", check_crc, crc, this->commands_[cmd_idx]->get_name().c_str());
      skip_next_phases(true);
    } else if (this->result_package_.size + data_size > sizeof(this->result_package_.buff)) {
      ESP_LOGW(TAG, "Received too big packet (%d bytes, but max size is %d bytes) for command [%s]", this->result_package_.size + data_size, sizeof(this->result_package_.buff), this->commands_[cmd_idx]->get_name().c_str());
      skip_next_phases(true);
    }
    // all validations passed
    ESP_LOGV(TAG, "Packet OK (checksum 0x%04X, data size %d bytes) for command [%s]", crc, data_size, this->commands_[cmd_idx]->get_name().c_str());
    meter.format = format;
    meter.rrr = (this->rx_package_.header.control >> 5) & 0x07;
    meter.sss = (this->rx_package_.header.control >> 1) & 0x07;
    memcpy(this->result_package_.buff + this->result_package_.size, this->rx_package_.data + 2, data_size);
    this->result_package_.size += data_size;
  } break;

  case 8: { // processing command
    if (meter.format.segmentation) {
      ESP_LOGV(TAG, "Packet with segmentation for command [%s]. Sending notification command for next frame", this->commands_[cmd_idx]->get_name().c_str());
      memset(&this->tx_package_, 0, sizeof(this->tx_package_));
      this->tx_bytes_length_ = this->commands_[cmd_idx]->fill_notification_request(&this->tx_package_);
      return_to_phase(2);
    } else {
      ESP_LOGV(TAG, "Processing %d bytes result for command [%s]", this->result_package_.size, this->commands_[cmd_idx]->get_name().c_str());
      this->result_package_.complete = true;
      if (!this->commands_[cmd_idx]->process_result(&this->rx_package_.header, &this->result_package_)) {
        ESP_LOGW(TAG, "Failed to process result for command [%s]", this->commands_[cmd_idx]->get_name().c_str());
        skip_next_phases(true);
      }
    }
  } break;

  case 9: { // publishing results until publish_result() return `true`
    ESP_LOGV(TAG, "Publishing result for command [%s]", this->commands_[cmd_idx]->get_name().c_str());
    if (!this->commands_[cmd_idx]->publish_result())
      return;
  } break;

  } // end switch

  this->phase_++;
}


void Nartis100::update() {
  if (this->phase_ != 0) {
    ESP_LOGW(TAG, "Skip update() coz previous was not finished!");
  } else {
    ESP_LOGV(TAG, "Time to Update");
    this->error_ = false; // reset error
    this->phase_ = 1;
  }
}

} // namespace nartis100
} // namespace esphome
