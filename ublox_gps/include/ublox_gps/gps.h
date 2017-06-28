//==============================================================================
// Copyright (c) 2012, Johannes Meyer, TU Darmstadt
// All rights reserved.

// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of the Flight Systems and Automatic Control group,
//       TU Darmstadt, nor the names of its contributors may be used to
//       endorse or promote products derived from this software without
//       specific prior written permission.

// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//==============================================================================

#ifndef UBLOX_GPS_H
#define UBLOX_GPS_H

#include <boost/asio/io_service.hpp>
#include <map>
#include <vector>

#include <ros/console.h>
#include <ublox/serialization/ublox_msgs.h>
#include <ublox_gps/async_worker.h>
#include <ublox_gps/callback.h>

#include <ublox_msgs/CfgNAV5.h>
#include <ublox_msgs/CfgNAVX5.h>
#include <ublox_msgs/CfgPRT.h>
#include <ublox_msgs/CfgRATE.h>
#include <ublox_msgs/CfgTMODE3.h>
#include <locale>
#include <stdexcept>

namespace ublox_gps {

/**
 * @brief Determine dynamic model from human-readable string.
 * @param model One of the following (case-insensitive):
 *  - portable
 *  - stationary
 *  - pedestrian
 *  - automotive
 *  - sea
 *  - airborne1
 *  - airborne2
 *  - airborne4
 *  - wristwatch
 * @return DynamicModel
 * @throws std::runtime_error on invalid argument.
 */
uint8_t
modelFromString(const std::string& model);

/**
 * @brief Determine fix mode from human-readable string.
 * @param mode One of the following (case-insensitive):
 *  - 2d
 *  - 3d
 *  - auto
 * @return FixMode
 * @throws std::runtime_error on invalid argument.
 */
uint8_t fixModeFromString(const std::string& mode);

class Gps {
 public:
  const static int kSetBaudrateSleepMs = 500;
  const static double kDefaultAckTimeout = 1.0;
  const static int kWriterSize = 1024;

  Gps();
  virtual ~Gps();

  template <typename StreamT>
  void initialize(StreamT& stream, boost::asio::io_service& io_service, 
                  unsigned int baudrate,
                  uint16_t uart_in,
                  uint16_t uart_out);
  void initialize(const boost::shared_ptr<Worker>& worker);
  void close();

  /**
   * @brief Configure the DGNSS mode (see CfgDGNSS message for details).
   * @param mode the DGNSS mode (see CfgDGNSS message for options)
   * @return true on ACK, false on other conditions
   */
  bool configDgnss(uint8_t mode);
  
  /**
   * @brief Configure the device navigation and measurement rate settings.
   * @param meas_rate Period in milliseconds between subsequent measurements.
   * @param nav_rate the rate at which navigation solutions are generated by the
   * receiver in number measurement cycles
   * @return true on ACK, false on other conditions.
   */
  bool configRate(uint16_t meas_rate, uint16_t nav_rate);

  /**
   * @brief Configure the RTCM messages with the given IDs to the set rate.
   * @param ids the RTCM ids, valid range: 0:255
   * @param rate the send rate
   * @return true on ACK, false on other conditions.
   */
  bool configRtcm(std::vector<int> ids, unsigned int rate);

  /**
   * @brief Set the TMODE3 settings to fixed at the given antenna reference
   * point (ARP) position in either Latitude Longitude Altitude (LLA) or 
   * ECEF coordinates.
   * @param arp_position a vector of size 3 representing the ARP position in 
   * ECEF coordinates [m] or LLA coordinates [deg]
   * @param arp_position_hp a vector of size 3 a vector of size 3 representing  
   * the ARP position in ECEF coordinates [m] or LLA coordinates [deg]
   * @param lla_flag true if position is given in LAT/LON/ALT, false if ECEF
   * @param fixed_pos_acc Fixed position 3D accuracy [m]
   * @return true on ACK, false if settings are incorrect or on other conditions
   */
  bool configTmode3Fixed(bool lla_flag,
                         std::vector<float> arp_position, 
                         std::vector<float> arp_position_hp,
                         float fixed_pos_acc);

  /**
   * @brief Set the TMODE3 settings to survey-in.
   * @param svinMinDur Survey-in minimum duration [s]
   * @param svinAccLiit Survey-in position accuracy limit [m]
   * @return true on ACK, false on other conditions.
   */
  bool configTmode3SurveyIn(unsigned int svinMinDur, float svinAccLimit);

  /**
   * @brief Configure the UART1 Port.
   * @param ids the baudrate of the port
   * @param in_proto_mask the in protocol mask, see CfgPRT message
   * @param out_proto_mask the out protocol mask, see CfgPRT message
   * @return true on ACK, false on other conditions.
   */
  bool configUart1(unsigned int baudrate, int16_t in_proto_mask, 
                int16_t out_proto_mask);

  /**
   * @brief Disable the UART Port. Sets in/out protocol masks to 0. Does not 
   * modify other values.
   * @param initial_cfg an empty message which is filled with the previous
   * configuration parameters
   * @return true on ACK, false on other conditions.
   */
  bool disableUart(ublox_msgs::CfgPRT prev_cfg);

  /**
   * @brief Set the TMODE3 settings to disabled. Should only be called for
   * High Precision GNSS devices, otherwise the device will return a NACK.
   * @return true on ACK, false on other conditions.
   */
  bool disableTmode3();

  /**
   * @brief Set the rate of the given message
   * @param class_id the class identifier of the message
   * @param message_id the message identifier
   * @param rate the updated rate in Hz
   * @return true on ACK, false on other conditions.
   */
  bool setRate(uint8_t class_id, uint8_t message_id, unsigned int rate);

  /**
   * @brief Set the device dynamic model.
   * @param model Dynamic model to use. Consult ublox protocol spec for details.
   * @return true on ACK, false on other conditions.
   */
  bool setDynamicModel(uint8_t model);

  /**
   * @brief Set the device fix mode.
   * @param mode 2D only, 3D only or auto.
   * @return true on ACK, false on other conditions.
   */
  bool setFixMode(uint8_t mode);

  /**
   * @brief Set the dead reckoning time limit
   * @param limit Time limit in seconds.
   * @return true on ACK, false on other conditions.
   */
  bool setDeadReckonLimit(uint8_t limit);

  /**
   * @brief Enable or disable PPP (precise-point-positioning).
   * @param enabled If true, PPP is enabled.
   * @return true on ACK, false on other conditions.
   *
   * @note This is part of the expert settings. It is recommended you check
   * the ublox manual first.
   */
  bool setPPPEnabled(bool enabled);

  /**
   * @brief Enable or disable SBAS.
   * @param enabled If true, PPP is enabled.
   * @return true on ACK, false on other conditions.
   *
   * @note This is part of the expert settings. It is recommended you check
   * the ublox manual first. If the device does not have SBAS capabilities 
   * it will return a NACK.
   */
  bool enableSBAS(bool enabled, uint8_t usage, uint8_t max_sbas);

  /**
   * @brief Set the rate of the Ublox message & subscribe to the given message
   * @param rate the rate in Hz of the message
   * @return
   */
  template <typename T>
  Callbacks::iterator subscribe(typename CallbackHandler_<T>::Callback callback,
                                unsigned int rate);
  /**
   * @brief Subscribe to the given Ublox message
   * @return
   */
  template <typename T>
  Callbacks::iterator subscribe(
      typename CallbackHandler_<T>::Callback callback);
  template <typename T>
  bool read(T& message,
            const boost::posix_time::time_duration& timeout = default_timeout_);

  bool isInitialized() const { return worker_ != 0; }
  bool isConfigured() const { return isInitialized() && configured_; }
  bool isOpen() const { return worker_->isOpen(); }

  template <typename ConfigT>
  bool poll(ConfigT& message,
            const boost::posix_time::time_duration& timeout = default_timeout_);
  bool poll(uint8_t class_id, uint8_t message_id,
            const std::vector<uint8_t>& payload = std::vector<uint8_t>());
  template <typename ConfigT>

  /**
   * @brief Send the given configuration message.
   * @param message the configuration message
   * @param wait if true, wait for an ACK
   * @return true if message sent successfully and either ACK was received or 
   * wait was set to false
   */
  bool configure(const ConfigT& message, bool wait = true);

  /**
   * @brief Wait for an acknowledge message until the timeout
   * @param timeout maximum time to wait in seconds
   */
  void waitForAcknowledge(const boost::posix_time::time_duration& timeout);

 private:
  void readCallback(unsigned char* data, std::size_t& size);

  boost::shared_ptr<Worker> worker_;
  bool configured_;
  // TODO: this variable is not thread safe :'(
  enum { WAIT, ACK, NACK } acknowledge_; 
  unsigned int baudrate_;
  uint16_t uart_in_;
  uint16_t uart_out_;
  static boost::posix_time::time_duration default_timeout_;

  Callbacks callbacks_;
  boost::mutex callback_mutex_;

};

/**
 * @brief Initialize the worker and configure the Serial port.
 * @param baudrate the baud rate of the port in Hz
 * @param uart_in the ublox UART 1 port in protocol (see Cfg PRT message)
 * @param uart_out the ublox UART 1 port out protocol (see Cfg PRT message)
 */
template <typename StreamT>
void Gps::initialize(StreamT& stream, boost::asio::io_service& io_service,
                     unsigned int baudrate,
                     uint16_t uart_in,
                     uint16_t uart_out) {
  if (worker_) return;
  initialize(
      boost::shared_ptr<Worker>(new AsyncWorker<StreamT>(stream, io_service)));
}

template <>
void Gps::initialize(boost::asio::serial_port& serial_port,
                     boost::asio::io_service& io_service,
                     unsigned int baudrate,
                     uint16_t uart_in,
                     uint16_t uart_out);

extern template void Gps::initialize<boost::asio::ip::tcp::socket>(
    boost::asio::ip::tcp::socket& stream, 
    boost::asio::io_service& io_service,
    unsigned int baudrate,
    uint16_t uart_in,
    uint16_t uart_out);
// extern template void
// Gps::initialize<boost::asio::ip::udp::socket>(boost::asio::ip::udp::socket&
// stream, boost::asio::io_service& io_service);

template <typename T>
Callbacks::iterator Gps::subscribe(
    typename CallbackHandler_<T>::Callback callback, unsigned int rate) {
  if (!setRate(T::CLASS_ID, T::MESSAGE_ID, rate)) return Callbacks::iterator();
  return subscribe<T>(callback);
}

template <typename T>
Callbacks::iterator Gps::subscribe(
    typename CallbackHandler_<T>::Callback callback) {
  boost::mutex::scoped_lock lock(callback_mutex_);
  CallbackHandler_<T>* handler = new CallbackHandler_<T>(callback);
  return callbacks_.insert(
      std::make_pair(std::make_pair(T::CLASS_ID, T::MESSAGE_ID),
                     boost::shared_ptr<CallbackHandler>(handler)));
}

template <typename ConfigT>
bool Gps::poll(ConfigT& message,
               const boost::posix_time::time_duration& timeout) {
  if (!poll(ConfigT::CLASS_ID, ConfigT::MESSAGE_ID)) return false;
  return read(message, timeout);
}

template <typename T>
bool Gps::read(T& message, const boost::posix_time::time_duration& timeout) {
  bool result = false;
  if (!worker_) return false;

  callback_mutex_.lock();
  CallbackHandler_<T>* handler = new CallbackHandler_<T>();
  Callbacks::iterator callback = callbacks_.insert(
      (std::make_pair(std::make_pair(T::CLASS_ID, T::MESSAGE_ID),
                      boost::shared_ptr<CallbackHandler>(handler))));
  callback_mutex_.unlock();

  if (handler->wait(timeout)) {
    message = handler->get();
    result = true;
  }

  callback_mutex_.lock();
  callbacks_.erase(callback);
  callback_mutex_.unlock();
  return result;
}

template <typename ConfigT>
bool Gps::configure(const ConfigT& message, bool wait) {
  if (!worker_) return false;

  acknowledge_ = WAIT;

  std::vector<unsigned char> out(kWriterSize);
  ublox::Writer writer(out.data(), out.size());
  if (!writer.write(message))
    return false;
  worker_->send(out.data(), writer.end() - out.data());

  if (!wait) return true;

  waitForAcknowledge(default_timeout_);
  return (acknowledge_ == ACK);
}

}  // namespace ublox_gps

#endif  // UBLOX_GPS_H
