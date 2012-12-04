// Copyright (C) 2012 Internet Systems Consortium, Inc. ("ISC")
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
// OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

#include <config.h>

#include <asiolink/io_address.h>
#include <dhcp/dhcp4.h>
#include <dhcp/dhcp6.h>
#include <dhcp/option4_addrlst.h>
#include <dhcp/option6_addrlst.h>
#include <dhcp/option6_ia.h>
#include <dhcp/option6_iaaddr.h>
#include <dhcp/option6_int.h>
#include <dhcp/option6_int_array.h>
#include <dhcp/option_definition.h>
#include <exceptions/exceptions.h>

#include <boost/pointer_cast.hpp>
#include <boost/shared_ptr.hpp>
#include <gtest/gtest.h>

using namespace std;
using namespace isc;
using namespace isc::dhcp;
using namespace isc::util;

namespace {

/// @brief OptionDefinition test class.
///
/// This class does not do anything useful but we keep
/// it around for the future.
class OptionDefinitionTest : public ::testing::Test {
public:
    // @brief Constructor.
    OptionDefinitionTest() { }
};

// The purpose of this test is to verify that OptionDefinition
// constructor initializes its members correctly.
TEST_F(OptionDefinitionTest, constructor) {
    // Specify the option data type as string. This should get converted
    // to enum value returned by getType().
    OptionDefinition opt_def1("OPTION_CLIENTID", 1, "string");
    EXPECT_EQ("OPTION_CLIENTID", opt_def1.getName());

    EXPECT_EQ(1, opt_def1.getCode());
    EXPECT_EQ(OPT_STRING_TYPE,  opt_def1.getType());
    EXPECT_FALSE(opt_def1.getArrayType());
    EXPECT_NO_THROW(opt_def1.validate());

    // Specify the option data type as an enum value.
    OptionDefinition opt_def2("OPTION_RAPID_COMMIT", 14,
                              OPT_EMPTY_TYPE);
    EXPECT_EQ("OPTION_RAPID_COMMIT", opt_def2.getName());
    EXPECT_EQ(14, opt_def2.getCode());
    EXPECT_EQ(OPT_EMPTY_TYPE, opt_def2.getType());
    EXPECT_FALSE(opt_def2.getArrayType());
    EXPECT_NO_THROW(opt_def1.validate());

    // Check if it is possible to set that option is an array.
    OptionDefinition opt_def3("OPTION_NIS_SERVERS", 27,
                              OPT_IPV6_ADDRESS_TYPE,
                              true);
    EXPECT_EQ("OPTION_NIS_SERVERS", opt_def3.getName());
    EXPECT_EQ(27, opt_def3.getCode());
    EXPECT_EQ(OPT_IPV6_ADDRESS_TYPE, opt_def3.getType());
    EXPECT_TRUE(opt_def3.getArrayType());
    EXPECT_NO_THROW(opt_def3.validate());

    // The created object is invalid if invalid data type is specified but
    // constructor shouldn't throw exception. The object is validated after
    // it has been created.
    EXPECT_NO_THROW(
        OptionDefinition opt_def4("OPTION_SERVERID",
                                  OPT_UNKNOWN_TYPE + 10,
                                  OPT_STRING_TYPE);
    );
}

// The purpose of this test is to verify that various data fields
// can be specified for an option definition when this definition
// is marked as 'record' and that fields can't be added if option
// definition is not marked as 'record'.
TEST_F(OptionDefinitionTest, addRecordField) {
    // We can only add fields to record if the option type has been
    // specified as 'record'. We try all other types but 'record'
    // here and expect exception to be thrown.
    for (int i = 0; i < OPT_UNKNOWN_TYPE; ++i) {
        // Do not try for 'record' type because this is the only
        // type for which adding record will succeed.
        if (i == OPT_RECORD_TYPE) {
            continue;
        }
        OptionDefinition opt_def("OPTION_IAADDR", 5,
                                 static_cast<OptionDataType>(i));
        EXPECT_THROW(opt_def.addRecordField("uint8"), isc::InvalidOperation);
    }

    // Positive scenario starts here.
    OptionDefinition opt_def("OPTION_IAADDR", 5, "record");
    EXPECT_NO_THROW(opt_def.addRecordField("ipv6-address"));
    EXPECT_NO_THROW(opt_def.addRecordField("uint32"));
    // It should not matter if we specify field type by its name or using enum.
    EXPECT_NO_THROW(opt_def.addRecordField(OPT_UINT32_TYPE));

    // Check what we have actually added.
    OptionDefinition::RecordFieldsCollection fields = opt_def.getRecordFields();
    ASSERT_EQ(3, fields.size());
    EXPECT_EQ(OPT_IPV6_ADDRESS_TYPE, fields[0]);
    EXPECT_EQ(OPT_UINT32_TYPE, fields[1]);
    EXPECT_EQ(OPT_UINT32_TYPE, fields[2]);

    // Let's try some more negative scenarios: use invalid data types.
    EXPECT_THROW(opt_def.addRecordField("unknown_type_xyz"), isc::BadValue);
    OptionDataType invalid_type =
        static_cast<OptionDataType>(OPT_UNKNOWN_TYPE + 10);
    EXPECT_THROW(opt_def.addRecordField(invalid_type), isc::BadValue);

    // It is bad if we use 'record' option type but don't specify
    // at least two fields.
    OptionDefinition opt_def2("OPTION_EMPTY_RECORD", 100, "record");
    EXPECT_THROW(opt_def2.validate(), MalformedOptionDefinition);
    opt_def2.addRecordField("uint8");
    EXPECT_THROW(opt_def2.validate(), MalformedOptionDefinition);
    opt_def2.addRecordField("uint32");
    EXPECT_NO_THROW(opt_def2.validate());
}

// The purpose of this test is to check that validate() function
// reports errors for invalid option definitions.
TEST_F(OptionDefinitionTest, validate) {
    // Not supported option type string is not allowed.
    OptionDefinition opt_def1("OPTION_CLIENTID", D6O_CLIENTID, "non-existent-type");
    EXPECT_THROW(opt_def1.validate(), MalformedOptionDefinition);

    // Not supported option type enum value is not allowed.
    OptionDefinition opt_def2("OPTION_CLIENTID", D6O_CLIENTID, OPT_UNKNOWN_TYPE);
    EXPECT_THROW(opt_def2.validate(), MalformedOptionDefinition);

    OptionDefinition opt_def3("OPTION_CLIENTID", D6O_CLIENTID,
                              static_cast<OptionDataType>(OPT_UNKNOWN_TYPE
                                                                      + 2));
    EXPECT_THROW(opt_def3.validate(), MalformedOptionDefinition);

    // Empty option name is not allowed.
    OptionDefinition opt_def4("", D6O_CLIENTID, "string");
    EXPECT_THROW(opt_def4.validate(), MalformedOptionDefinition);

    // Option name must not contain spaces.
    OptionDefinition opt_def5(" OPTION_CLIENTID", D6O_CLIENTID, "string");
    EXPECT_THROW(opt_def5.validate(), MalformedOptionDefinition);

    // Option name must not contain spaces.
    OptionDefinition opt_def6("OPTION CLIENTID", D6O_CLIENTID, "string", true);
    EXPECT_THROW(opt_def6.validate(), MalformedOptionDefinition);

    // Having array of strings does not make sense because there is no way
    // to determine string's length.
    OptionDefinition opt_def7("OPTION_CLIENTID", D6O_CLIENTID, "string", true);
    EXPECT_THROW(opt_def7.validate(), MalformedOptionDefinition);

    // It does not make sense to have string field within the record before
    // other fields because there is no way to determine the length of this
    // string and thus there is no way to determine where the other field
    // begins.
    OptionDefinition opt_def8("OPTION_STATUS_CODE", D6O_STATUS_CODE,
                              "record");
    opt_def8.addRecordField("string");
    opt_def8.addRecordField("uint16");
    EXPECT_THROW(opt_def8.validate(), MalformedOptionDefinition);

    // ... but it is ok if the string value is the last one.
    OptionDefinition opt_def9("OPTION_STATUS_CODE", D6O_STATUS_CODE,
                              "record");
    opt_def9.addRecordField("uint8");
    opt_def9.addRecordField("string");
}


// The purpose of this test is to verify that option definition
// that comprises array of IPv6 addresses will return an instance
// of option with a list of IPv6 addresses.
TEST_F(OptionDefinitionTest, ipv6AddressArray) {
    OptionDefinition opt_def("OPTION_NIS_SERVERS", D6O_NIS_SERVERS,
                             "ipv6-address", true);

    // Create a list of some V6 addresses.
    std::vector<asiolink::IOAddress> addrs;
    addrs.push_back(asiolink::IOAddress("2001:0db8::ff00:0042:8329"));
    addrs.push_back(asiolink::IOAddress("2001:0db8::ff00:0042:2319"));
    addrs.push_back(asiolink::IOAddress("::1"));
    addrs.push_back(asiolink::IOAddress("::2"));

    // Write addresses to the buffer.
    OptionBuffer buf(addrs.size() * asiolink::V6ADDRESS_LEN);
    for (int i = 0; i < addrs.size(); ++i) {
        asio::ip::address_v6::bytes_type addr_bytes =
            addrs[i].getAddress().to_v6().to_bytes();
        ASSERT_EQ(asiolink::V6ADDRESS_LEN, addr_bytes.size());
        std::copy(addr_bytes.begin(), addr_bytes.end(),
                  buf.begin() + i * asiolink::V6ADDRESS_LEN);
    }
    // Create DHCPv6 option from this buffer. Once option is created it is
    // supposed to have internal list of addresses that it parses out from
    // the provided buffer.
    OptionPtr option_v6;
    ASSERT_NO_THROW(
        option_v6 = opt_def.optionFactory(Option::V6, D6O_NIS_SERVERS, buf);
    );
    ASSERT_TRUE(typeid(*option_v6) == typeid(Option6AddrLst));
    boost::shared_ptr<Option6AddrLst> option_cast_v6 =
        boost::static_pointer_cast<Option6AddrLst>(option_v6);
    ASSERT_TRUE(option_cast_v6);
    // Get the list of parsed addresses from the option object.
    std::vector<asiolink::IOAddress> addrs_returned =
        option_cast_v6->getAddresses();
    // The list of addresses must exactly match addresses that we
    // stored in the buffer to create the option from it.
    EXPECT_TRUE(std::equal(addrs.begin(), addrs.end(), addrs_returned.begin()));

    // The provided buffer's length must be a multiple of V6 address length.
    // Let's extend the buffer by one byte so as this condition is not
    // fulfilled anymore.
    buf.insert(buf.end(), 1, 1);
    // It should throw exception then.
    EXPECT_THROW(
        opt_def.optionFactory(Option::V6, D6O_NIS_SERVERS, buf),
        InvalidOptionValue
    );
}

// The purpose of this test is to verify that option definition
// that comprises array of IPv6 addresses will return an instance
// of option with a list of IPv6 addresses. Array of IPv6 addresses
// is specified as a vector of strings (each string represents single
// IPv6 address).
TEST_F(OptionDefinitionTest, ipv6AddressArrayTokenized) {
    OptionDefinition opt_def("OPTION_NIS_SERVERS", D6O_NIS_SERVERS,
                             "ipv6-address", true);

    // Create a vector of some V6 addresses.
    std::vector<asiolink::IOAddress> addrs;
    addrs.push_back(asiolink::IOAddress("2001:0db8::ff00:0042:8329"));
    addrs.push_back(asiolink::IOAddress("2001:0db8::ff00:0042:2319"));
    addrs.push_back(asiolink::IOAddress("::1"));
    addrs.push_back(asiolink::IOAddress("::2"));

    // Create a vector of strings representing addresses given above.
    std::vector<std::string> addrs_str;
    for (std::vector<asiolink::IOAddress>::const_iterator it = addrs.begin();
         it != addrs.end(); ++it) {
        addrs_str.push_back(it->toText());
    }

    // Create DHCPv6 option using the list of IPv6 addresses given in the
    // string form.
    OptionPtr option_v6;
    ASSERT_NO_THROW(
        option_v6 = opt_def.optionFactory(Option::V6, D6O_NIS_SERVERS,
                                          addrs_str);
    );
    // Non-null pointer option is supposed to be returned and it
    // should have Option6AddrLst type.
    ASSERT_TRUE(option_v6);
    ASSERT_TRUE(typeid(*option_v6) == typeid(Option6AddrLst));
    // Cast to the actual option type to get IPv6 addresses from it.
    boost::shared_ptr<Option6AddrLst> option_cast_v6 =
        boost::static_pointer_cast<Option6AddrLst>(option_v6);
    // Check that cast was successful.
    ASSERT_TRUE(option_cast_v6);
    // Get the list of parsed addresses from the option object.
    std::vector<asiolink::IOAddress> addrs_returned =
        option_cast_v6->getAddresses();
    // Returned addresses must match the addresses that have been used to create
    // the option instance.
    EXPECT_TRUE(std::equal(addrs.begin(), addrs.end(), addrs_returned.begin()));
}

// The purpose of this test is to verify that option definition
// that comprises array of IPv4 addresses will return an instance
// of option with a list of IPv4 addresses.
TEST_F(OptionDefinitionTest, ipv4AddressArray) {
    OptionDefinition opt_def("OPTION_NAME_SERVERS", D6O_NIS_SERVERS,
                             "ipv4-address", true);

    // Create a list of some V6 addresses.
    std::vector<asiolink::IOAddress> addrs;
    addrs.push_back(asiolink::IOAddress("192.168.0.1"));
    addrs.push_back(asiolink::IOAddress("172.16.1.1"));
    addrs.push_back(asiolink::IOAddress("127.0.0.1"));
    addrs.push_back(asiolink::IOAddress("213.41.23.12"));

    // Write addresses to the buffer.
    OptionBuffer buf(addrs.size() * asiolink::V4ADDRESS_LEN);
    for (int i = 0; i < addrs.size(); ++i) {
        asio::ip::address_v4::bytes_type addr_bytes =
            addrs[i].getAddress().to_v4().to_bytes();
        ASSERT_EQ(asiolink::V4ADDRESS_LEN, addr_bytes.size());
        std::copy(addr_bytes.begin(), addr_bytes.end(),
                  buf.begin() + i * asiolink::V4ADDRESS_LEN);
    }
    // Create DHCPv6 option from this buffer. Once option is created it is
    // supposed to have internal list of addresses that it parses out from
    // the provided buffer.
    OptionPtr option_v4;
    ASSERT_NO_THROW(
        option_v4 = opt_def.optionFactory(Option::V4, DHO_NAME_SERVERS, buf)
    );
    ASSERT_TRUE(typeid(*option_v4) == typeid(Option4AddrLst));
    // Get the list of parsed addresses from the option object.
    boost::shared_ptr<Option4AddrLst> option_cast_v4 =
        boost::static_pointer_cast<Option4AddrLst>(option_v4);
    std::vector<asiolink::IOAddress> addrs_returned =
        option_cast_v4->getAddresses();
    // The list of addresses must exactly match addresses that we
    // stored in the buffer to create the option from it.
    EXPECT_TRUE(std::equal(addrs.begin(), addrs.end(), addrs_returned.begin()));

    // The provided buffer's length must be a multiple of V4 address length.
    // Let's extend the buffer by one byte so as this condition is not
    // fulfilled anymore.
    buf.insert(buf.end(), 1, 1);
    // It should throw exception then.
    EXPECT_THROW(opt_def.optionFactory(Option::V4, DHO_NIS_SERVERS, buf),
                 InvalidOptionValue);
}

// The purpose of this test is to verify that option definition
// that comprises array of IPv4 addresses will return an instance
// of option with a list of IPv4 addresses. The array of IPv4 addresses
// is specified as a vector of strings (each string represents single
// IPv4 address).
TEST_F(OptionDefinitionTest, ipv4AddressArrayTokenized) {
    OptionDefinition opt_def("OPTION_NIS_SERVERS", DHO_NIS_SERVERS,
                             "ipv4-address", true);

    // Create a vector of some V6 addresses.
    std::vector<asiolink::IOAddress> addrs;
    addrs.push_back(asiolink::IOAddress("192.168.0.1"));
    addrs.push_back(asiolink::IOAddress("172.16.1.1"));
    addrs.push_back(asiolink::IOAddress("127.0.0.1"));
    addrs.push_back(asiolink::IOAddress("213.41.23.12"));

    // Create a vector of strings representing addresses given above.
    std::vector<std::string> addrs_str;
    for (std::vector<asiolink::IOAddress>::const_iterator it = addrs.begin();
         it != addrs.end(); ++it) {
        addrs_str.push_back(it->toText());
    }

    // Create DHCPv4 option using the list of IPv4 addresses given in the
    // string form.
    OptionPtr option_v4;
    ASSERT_NO_THROW(
        option_v4 = opt_def.optionFactory(Option::V4, DHO_NIS_SERVERS,
                                          addrs_str);
    );
    // Non-null pointer option is supposed to be returned and it
    // should have Option6AddrLst type.
    ASSERT_TRUE(option_v4);
    ASSERT_TRUE(typeid(*option_v4) == typeid(Option4AddrLst));
    // Cast to the actual option type to get IPv4 addresses from it.
    boost::shared_ptr<Option4AddrLst> option_cast_v4 =
        boost::static_pointer_cast<Option4AddrLst>(option_v4);
    // Check that cast was successful.
    ASSERT_TRUE(option_cast_v4);
    // Get the list of parsed addresses from the option object.
    std::vector<asiolink::IOAddress> addrs_returned =
        option_cast_v4->getAddresses();
    // Returned addresses must match the addresses that have been used to create
    // the option instance.
    EXPECT_TRUE(std::equal(addrs.begin(), addrs.end(), addrs_returned.begin()));
}

// The purpose of thie test is to verify that option definition for
// 'empty' option can be created and that it returns 'empty' option.
TEST_F(OptionDefinitionTest, empty) {
    OptionDefinition opt_def("OPTION_RAPID_COMMIT", D6O_RAPID_COMMIT, "empty");

    // Create option instance and provide empty buffer as expected.
    OptionPtr option_v6;
    ASSERT_NO_THROW(
        option_v6 = opt_def.optionFactory(Option::V6, D6O_RAPID_COMMIT, OptionBuffer())
    );
    ASSERT_TRUE(typeid(*option_v6) == typeid(Option));
    // Expect 'empty' DHCPv6 option.
    EXPECT_EQ(Option::V6, option_v6->getUniverse());
    EXPECT_EQ(4, option_v6->getHeaderLen());
    EXPECT_EQ(0, option_v6->getData().size());

    // Repeat the same test scenario for DHCPv4 option.
    OptionPtr option_v4;
    ASSERT_NO_THROW(option_v4 = opt_def.optionFactory(Option::V4, 214, OptionBuffer()));
    // Expect 'empty' DHCPv4 option.
    EXPECT_EQ(Option::V4, option_v4->getUniverse());
    EXPECT_EQ(2, option_v4->getHeaderLen());
    EXPECT_EQ(0, option_v4->getData().size());
}

// The purpose of this test is to verify that definition can be
// creates for the option that holds binary data.
TEST_F(OptionDefinitionTest, binary) {
    // Binary option is the one that is represented by the generic
    // Option class. In fact all options can be represented by this
    // class but for some of them it is just natural. The SERVERID
    // option consists of the option code, length and binary data so
    // this one was picked for this test.
    OptionDefinition opt_def("OPTION_SERVERID", D6O_SERVERID, "binary");

    // Prepare some dummy data (serverid): 0, 1, 2 etc.
    OptionBuffer buf(14);
    for (int i = 0; i < 14; ++i) {
        buf[i] = i;
    }
    // Create option instance with the factory function.
    // If the OptionDefinition code works properly than
    // object of the type Option should be returned.
    OptionPtr option_v6;
    ASSERT_NO_THROW(
        option_v6 = opt_def.optionFactory(Option::V6, D6O_SERVERID, buf);
    );
    // Expect base option type returned.
    ASSERT_TRUE(typeid(*option_v6) == typeid(Option));
    // Sanity check on universe, length and size. These are
    // the basic parameters identifying any option.
    EXPECT_EQ(Option::V6, option_v6->getUniverse());
    EXPECT_EQ(4, option_v6->getHeaderLen());
    ASSERT_EQ(buf.size(), option_v6->getData().size());

    // Get the server id data from the option and compare
    // against reference buffer. They are expected to match.
    EXPECT_TRUE(std::equal(option_v6->getData().begin(),
                           option_v6->getData().end(),
                           buf.begin()));

    // Repeat the same test scenario for DHCPv4 option.
    OptionPtr option_v4;
    ASSERT_NO_THROW(option_v4 = opt_def.optionFactory(Option::V4, 214, buf));
    // Expect 'empty' DHCPv4 option.
    EXPECT_EQ(Option::V4, option_v4->getUniverse());
    EXPECT_EQ(2, option_v4->getHeaderLen());
    ASSERT_EQ(buf.size(), option_v4->getData().size());

    EXPECT_TRUE(std::equal(option_v6->getData().begin(),
                           option_v6->getData().end(),
                           buf.begin()));
}

// The purpose of this test is to verify that definition can be created
// for option that comprises record of data. In this particular test
// the IA_NA option is used. This option comprises three uint32 fields.
TEST_F(OptionDefinitionTest, recordIA6) {
    // This option consists of IAID, T1 and T2 fields (each 4 bytes long).
    const int option6_ia_len = 12;

    // Get the factory function pointer.
    OptionDefinition opt_def("OPTION_IA_NA", D6O_IA_NA, "record", false);
    // Each data field is uint32.
    for (int i = 0; i < 3; ++i) {
        EXPECT_NO_THROW(opt_def.addRecordField("uint32"));
    }

    // Check the positive scenario.
    OptionBuffer buf(12);
    for (int i = 0; i < buf.size(); ++i) {
        buf[i] = i;
    }
    OptionPtr option_v6;
    ASSERT_NO_THROW(option_v6 = opt_def.optionFactory(Option::V6, D6O_IA_NA, buf));
    ASSERT_TRUE(typeid(*option_v6) == typeid(Option6IA));
    boost::shared_ptr<Option6IA> option_cast_v6 =
        boost::static_pointer_cast<Option6IA>(option_v6);
    EXPECT_EQ(0x00010203, option_cast_v6->getIAID());
    EXPECT_EQ(0x04050607, option_cast_v6->getT1());
    EXPECT_EQ(0x08090A0B, option_cast_v6->getT2());

    // The length of the buffer must be at least 12 bytes.
    // Check too short buffer.
    EXPECT_THROW(
        opt_def.optionFactory(Option::V6, D6O_IA_NA, OptionBuffer(option6_ia_len - 1)),
        InvalidOptionValue
     );
}

// The purpose of this test is to verify that definition can be created
// for option that comprises record of data. In this particular test
// the IAADDR option is used.
TEST_F(OptionDefinitionTest, recordIAAddr6) {
    // This option consists of IPV6 Address (16 bytes) and preferred-lifetime and
    // valid-lifetime fields (each 4 bytes long).
    const int option6_iaaddr_len = 24;

    OptionDefinition opt_def("OPTION_IAADDR", D6O_IAADDR, "record");
    ASSERT_NO_THROW(opt_def.addRecordField("ipv6-address"));
    ASSERT_NO_THROW(opt_def.addRecordField("uint32"));
    ASSERT_NO_THROW(opt_def.addRecordField("uint32"));

    // Check the positive scenario.
    OptionPtr option_v6;
    asiolink::IOAddress addr_v6("2001:0db8::ff00:0042:8329");
    OptionBuffer buf(asiolink::V6ADDRESS_LEN);
    ASSERT_TRUE(addr_v6.getAddress().is_v6());
    asio::ip::address_v6::bytes_type addr_bytes =
        addr_v6.getAddress().to_v6().to_bytes();
    ASSERT_EQ(asiolink::V6ADDRESS_LEN, addr_bytes.size());
    std::copy(addr_bytes.begin(), addr_bytes.end(), buf.begin());

    for (int i = 0; i < option6_iaaddr_len - asiolink::V6ADDRESS_LEN; ++i) {
        buf.push_back(i);
    }
    ASSERT_NO_THROW(option_v6 = opt_def.optionFactory(Option::V6, D6O_IAADDR, buf));
    ASSERT_TRUE(typeid(*option_v6) == typeid(Option6IAAddr));
    boost::shared_ptr<Option6IAAddr> option_cast_v6 =
        boost::static_pointer_cast<Option6IAAddr>(option_v6);
    EXPECT_EQ(addr_v6, option_cast_v6->getAddress());
    EXPECT_EQ(0x00010203, option_cast_v6->getPreferred());
    EXPECT_EQ(0x04050607, option_cast_v6->getValid());

    // The length of the buffer must be at least 12 bytes.
    // Check too short buffer.
    EXPECT_THROW(
        opt_def.optionFactory(Option::V6, D6O_IAADDR, OptionBuffer(option6_iaaddr_len - 1)),
        InvalidOptionValue
     );
}

// The purpose of this test is to verify that definition can be created
// for option that comprises record of data. In this particular test
// the IAADDR option is used. The data for the option is speicifed as
// a vector of strings. Each string carries the data for the corresponding
// data field.
TEST_F(OptionDefinitionTest, recordIAAddr6Tokenized) {
    // This option consists of IPV6 Address (16 bytes) and preferred-lifetime and
    // valid-lifetime fields (each 4 bytes long).
    OptionDefinition opt_def("OPTION_IAADDR", D6O_IAADDR, "record");
    ASSERT_NO_THROW(opt_def.addRecordField("ipv6-address"));
    ASSERT_NO_THROW(opt_def.addRecordField("uint32"));
    ASSERT_NO_THROW(opt_def.addRecordField("uint32"));

    // Check the positive scenario.
    std::vector<std::string> data_field_values;
    data_field_values.push_back("2001:0db8::ff00:0042:8329");
    data_field_values.push_back("1234");
    data_field_values.push_back("5678");

    OptionPtr option_v6;
    ASSERT_NO_THROW(option_v6 = opt_def.optionFactory(Option::V6, D6O_IAADDR,
                                                      data_field_values));
    ASSERT_TRUE(typeid(*option_v6) == typeid(Option6IAAddr));
    boost::shared_ptr<Option6IAAddr> option_cast_v6 =
        boost::static_pointer_cast<Option6IAAddr>(option_v6);
    EXPECT_EQ("2001:db8::ff00:42:8329", option_cast_v6->getAddress().toText());
    EXPECT_EQ(1234, option_cast_v6->getPreferred());
    EXPECT_EQ(5678, option_cast_v6->getValid());
}

// The purpose of this test is to verify that definition for option that
// comprises single uint8 value can be created and that this definition
// can be used to create an option with single uint8 value.
TEST_F(OptionDefinitionTest, uint8) {
    OptionDefinition opt_def("OPTION_PREFERENCE", D6O_PREFERENCE, "uint8");

    OptionPtr option_v6;
    // Try to use correct buffer length = 1 byte.
    ASSERT_NO_THROW(
        option_v6 = opt_def.optionFactory(Option::V6, D6O_PREFERENCE, OptionBuffer(1, 1));
    );
    ASSERT_TRUE(typeid(*option_v6) == typeid(Option6Int<uint8_t>));
    // Validate the value.
    boost::shared_ptr<Option6Int<uint8_t> > option_cast_v6 =
        boost::static_pointer_cast<Option6Int<uint8_t> >(option_v6);
    EXPECT_EQ(1, option_cast_v6->getValue());

    // Try to provide zero-length buffer. Expect exception.
    EXPECT_THROW(
        option_v6 = opt_def.optionFactory(Option::V6, D6O_PREFERENCE, OptionBuffer()),
        InvalidOptionValue
    );

    // @todo Add more cases for DHCPv4
}

// The purpose of this test is to verify that definition for option that
// comprises single uint8 value can be created and that this definition
// can be used to create an option with single uint8 value.
TEST_F(OptionDefinitionTest, uint8Tokenized) {
    OptionDefinition opt_def("OPTION_PREFERENCE", D6O_PREFERENCE, "uint8");

    OptionPtr option_v6;
    std::vector<std::string> values;
    values.push_back("123");
    values.push_back("456");
    ASSERT_NO_THROW(
        option_v6 = opt_def.optionFactory(Option::V6, D6O_PREFERENCE, values);
    );
    ASSERT_TRUE(typeid(*option_v6) == typeid(Option6Int<uint8_t>));
    // Validate the value.
    boost::shared_ptr<Option6Int<uint8_t> > option_cast_v6 =
        boost::static_pointer_cast<Option6Int<uint8_t> >(option_v6);
    EXPECT_EQ(123, option_cast_v6->getValue());

    // @todo Add more cases for DHCPv4
}

// The purpose of this test is to verify that definition for option that
// comprises single uint16 value can be created and that this definition
// can be used to create an option with single uint16 value.
TEST_F(OptionDefinitionTest, uint16) {
    OptionDefinition opt_def("OPTION_ELAPSED_TIME", D6O_ELAPSED_TIME, "uint16");

    OptionPtr option_v6;
    // Try to use correct buffer length = 2 bytes.
    OptionBuffer buf;
    buf.push_back(1);
    buf.push_back(2);
    ASSERT_NO_THROW(
        option_v6 = opt_def.optionFactory(Option::V6, D6O_ELAPSED_TIME, buf);
    );
    ASSERT_TRUE(typeid(*option_v6) == typeid(Option6Int<uint16_t>));
    // Validate the value.
    boost::shared_ptr<Option6Int<uint16_t> > option_cast_v6 =
        boost::static_pointer_cast<Option6Int<uint16_t> >(option_v6);
    EXPECT_EQ(0x0102, option_cast_v6->getValue());

    // Try to provide zero-length buffer. Expect exception.
    EXPECT_THROW(
        option_v6 = opt_def.optionFactory(Option::V6, D6O_ELAPSED_TIME, OptionBuffer(1)),
        InvalidOptionValue
    );

    // @todo Add more cases for DHCPv4
}

// The purpose of this test is to verify that definition for option that
// comprises single uint16 value can be created and that this definition
// can be used to create an option with single uint16 value.
TEST_F(OptionDefinitionTest, uint16Tokenized) {
    OptionDefinition opt_def("OPTION_ELAPSED_TIME", D6O_ELAPSED_TIME, "uint16");

    OptionPtr option_v6;

    std::vector<std::string> values;
    values.push_back("1234");
    values.push_back("5678");
    ASSERT_NO_THROW(
        option_v6 = opt_def.optionFactory(Option::V6, D6O_ELAPSED_TIME, values);
    );
    ASSERT_TRUE(typeid(*option_v6) == typeid(Option6Int<uint16_t>));
    // Validate the value.
    boost::shared_ptr<Option6Int<uint16_t> > option_cast_v6 =
        boost::static_pointer_cast<Option6Int<uint16_t> >(option_v6);
    EXPECT_EQ(1234, option_cast_v6->getValue());

    // @todo Add more cases for DHCPv4

}

// The purpose of this test is to verify that definition for option that
// comprises single uint32 value can be created and that this definition
// can be used to create an option with single uint32 value.
TEST_F(OptionDefinitionTest, uint32) {
    OptionDefinition opt_def("OPTION_CLT_TIME", D6O_CLT_TIME, "uint32");

    OptionPtr option_v6;
    OptionBuffer buf;
    buf.push_back(1);
    buf.push_back(2);
    buf.push_back(3);
    buf.push_back(4);
    ASSERT_NO_THROW(
        option_v6 = opt_def.optionFactory(Option::V6, D6O_CLT_TIME, buf);
    );
    ASSERT_TRUE(typeid(*option_v6) == typeid(Option6Int<uint32_t>));
    // Validate the value.
    boost::shared_ptr<Option6Int<uint32_t> > option_cast_v6 =
        boost::static_pointer_cast<Option6Int<uint32_t> >(option_v6);
    EXPECT_EQ(0x01020304, option_cast_v6->getValue());

    // Try to provide too short buffer. Expect exception.
    EXPECT_THROW(
        option_v6 = opt_def.optionFactory(Option::V6, D6O_CLT_TIME, OptionBuffer(2)),
        InvalidOptionValue
    );

    // @todo Add more cases for DHCPv4
}

// The purpose of this test is to verify that definition for option that
// comprises single uint32 value can be created and that this definition
// can be used to create an option with single uint32 value.
TEST_F(OptionDefinitionTest, uint32Tokenized) {
    OptionDefinition opt_def("OPTION_CLT_TIME", D6O_CLT_TIME, "uint32");

    OptionPtr option_v6;
    std::vector<std::string> values;
    values.push_back("123456");
    values.push_back("789");
    ASSERT_NO_THROW(
        option_v6 = opt_def.optionFactory(Option::V6, D6O_CLT_TIME, values);
    );
    ASSERT_TRUE(typeid(*option_v6) == typeid(Option6Int<uint32_t>));
    // Validate the value.
    boost::shared_ptr<Option6Int<uint32_t> > option_cast_v6 =
        boost::static_pointer_cast<Option6Int<uint32_t> >(option_v6);
    EXPECT_EQ(123456, option_cast_v6->getValue());

    // @todo Add more cases for DHCPv4
}

// The purpose of this test is to verify that definition for option that
// comprises array of uint16 values can be created and that this definition
// can be used to create option with an array of uint16 values.
TEST_F(OptionDefinitionTest, uint16Array) {
    // Let's define some dummy option.
    const uint16_t opt_code = 79;
    OptionDefinition opt_def("OPTION_UINT16_ARRAY", opt_code, "uint16", true);

    OptionPtr option_v6;
    // Positive scenario, initiate the buffer with length being
    // multiple of uint16_t size.
    // buffer elements will be: 0x112233.
    OptionBuffer buf(6);
    for (int i = 0; i < 6; ++i) {
        buf[i] = i / 2;
    }
    // Constructor should succeed because buffer has correct size.
    EXPECT_NO_THROW(
        option_v6 = opt_def.optionFactory(Option::V6, opt_code, buf);
    );
    ASSERT_TRUE(typeid(*option_v6) == typeid(Option6IntArray<uint16_t>));
    boost::shared_ptr<Option6IntArray<uint16_t> > option_cast_v6 =
        boost::static_pointer_cast<Option6IntArray<uint16_t> >(option_v6);
    // Get the values from the initiated options and validate.
    std::vector<uint16_t> values = option_cast_v6->getValues();
    for (int i = 0; i < values.size(); ++i) {
        // Expected value is calculated using on the same pattern
        // as the one we used to initiate buffer:
        // for i=0, expected = 0x00, for i = 1, expected == 0x11 etc.
        uint16_t expected = (i << 8) | i;
        EXPECT_EQ(expected, values[i]);
    }

    // Provided buffer size must be greater than zero. Check if we
    // get exception if we provide zero-length buffer.
    EXPECT_THROW(
        option_v6 = opt_def.optionFactory(Option::V6, opt_code, OptionBuffer()),
        InvalidOptionValue
    );
    // Buffer length must be multiple of data type size.
    EXPECT_THROW(
        option_v6 = opt_def.optionFactory(Option::V6, opt_code, OptionBuffer(5)),
        InvalidOptionValue
    );
}

// The purpose of this test is to verify that definition for option that
// comprises array of uint16 values can be created and that this definition
// can be used to create option with an array of uint16 values.
TEST_F(OptionDefinitionTest, uint16ArrayTokenized) {
    // Let's define some dummy option.
    const uint16_t opt_code = 79;
    OptionDefinition opt_def("OPTION_UINT16_ARRAY", opt_code, "uint16", true);

    OptionPtr option_v6;
    std::vector<std::string> str_values;
    str_values.push_back("12345");
    str_values.push_back("5679");
    str_values.push_back("12");
    EXPECT_NO_THROW(
        option_v6 = opt_def.optionFactory(Option::V6, opt_code, str_values);
    );
    ASSERT_TRUE(typeid(*option_v6) == typeid(Option6IntArray<uint16_t>));
    boost::shared_ptr<Option6IntArray<uint16_t> > option_cast_v6 =
        boost::static_pointer_cast<Option6IntArray<uint16_t> >(option_v6);
    // Get the values from the initiated options and validate.
    std::vector<uint16_t> values = option_cast_v6->getValues();
    EXPECT_EQ(12345, values[0]);
    EXPECT_EQ(5679, values[1]);
    EXPECT_EQ(12, values[2]);
}

// The purpose of this test is to verify that definition for option that
// comprises array of uint32 values can be created and that this definition
// can be used to create option with an array of uint32 values.
TEST_F(OptionDefinitionTest, uint32Array) {
    // Let's define some dummy option.
    const uint16_t opt_code = 80;

    OptionDefinition opt_def("OPTION_UINT32_ARRAY", opt_code, "uint32", true);

    OptionPtr option_v6;
    // Positive scenario, initiate the buffer with length being
    // multiple of uint16_t size.
    // buffer elements will be: 0x111122223333.
    OptionBuffer buf(12);
    for (int i = 0; i < buf.size(); ++i) {
        buf[i] = i / 4;
    }
    // Constructor should succeed because buffer has correct size.
    EXPECT_NO_THROW(
        option_v6 = opt_def.optionFactory(Option::V6, opt_code, buf);
    );
    ASSERT_TRUE(typeid(*option_v6) == typeid(Option6IntArray<uint32_t>));
    boost::shared_ptr<Option6IntArray<uint32_t> > option_cast_v6 =
        boost::static_pointer_cast<Option6IntArray<uint32_t> >(option_v6);
    // Get the values from the initiated options and validate.
    std::vector<uint32_t> values = option_cast_v6->getValues();
    for (int i = 0; i < values.size(); ++i) {
        // Expected value is calculated using on the same pattern
        // as the one we used to initiate buffer:
        // for i=0, expected = 0x0000, for i = 1, expected == 0x1111 etc.
        uint32_t expected = 0x01010101 * i;
        EXPECT_EQ(expected, values[i]);
    }

    // Provided buffer size must be greater than zero. Check if we
    // get exception if we provide zero-length buffer.
    EXPECT_THROW(
        option_v6 = opt_def.optionFactory(Option::V6, opt_code, OptionBuffer()),
        InvalidOptionValue
    );
    // Buffer length must be multiple of data type size.
    EXPECT_THROW(
        option_v6 = opt_def.optionFactory(Option::V6, opt_code, OptionBuffer(5)),
        InvalidOptionValue
    );
}

// The purpose of this test is to verify that definition for option that
// comprises array of uint32 values can be created and that this definition
// can be used to create option with an array of uint32 values.
TEST_F(OptionDefinitionTest, uint32ArrayTokenized) {
    // Let's define some dummy option.
    const uint16_t opt_code = 80;

    OptionDefinition opt_def("OPTION_UINT32_ARRAY", opt_code, "uint32", true);

    OptionPtr option_v6;
    std::vector<std::string> str_values;
    str_values.push_back("123456");
    str_values.push_back("7");
    str_values.push_back("256");
    str_values.push_back("1111");

    EXPECT_NO_THROW(
        option_v6 = opt_def.optionFactory(Option::V6, opt_code, str_values);
    );
    ASSERT_TRUE(typeid(*option_v6) == typeid(Option6IntArray<uint32_t>));
    boost::shared_ptr<Option6IntArray<uint32_t> > option_cast_v6 =
        boost::static_pointer_cast<Option6IntArray<uint32_t> >(option_v6);
    // Get the values from the initiated options and validate.
    std::vector<uint32_t> values = option_cast_v6->getValues();
    EXPECT_EQ(123456, values[0]);
    EXPECT_EQ(7, values[1]);
    EXPECT_EQ(256, values[2]);
    EXPECT_EQ(1111, values[3]);
}

// The purpose of this test is to verify that the definition can be created
// for the option that comprises string value in the UTF8 format.
TEST_F(OptionDefinitionTest, utf8StringTokenized) {
    // Let's create some dummy option.
    const uint16_t opt_code = 80;
    OptionDefinition opt_def("OPTION_WITH_STRING", opt_code, "string");
    
    std::vector<std::string> values;
    values.push_back("Hello World");
    values.push_back("this string should not be included in the option");
    OptionPtr option_v6;
    EXPECT_NO_THROW(
        option_v6 = opt_def.optionFactory(Option::V6, opt_code, values);
    );
    ASSERT_TRUE(option_v6);
    ASSERT_TRUE(typeid(*option_v6) == typeid(Option));
    std::vector<uint8_t> data = option_v6->getData();
    std::vector<uint8_t> ref_data(values[0].c_str(), values[0].c_str()
                                  + values[0].length());
    EXPECT_TRUE(std::equal(ref_data.begin(), ref_data.end(), data.begin()));
}

// The purpose of this test is to check that non-integer data type can't
// be used for factoryInteger function.
TEST_F(OptionDefinitionTest, integerInvalidType) {
    // The template function factoryInteger<> accepts integer values only
    // as template typename. Here we try passing different type and
    // see if it rejects it.
    OptionBuffer buf(1);
    EXPECT_THROW(
        OptionDefinition::factoryInteger<bool>(Option::V6, D6O_PREFERENCE,
                                               buf.begin(), buf.end()),
        isc::dhcp::InvalidDataType
    );
}

// The purpose of this test is to verify that helper methods
// haveIA6Format and haveIAAddr6Format can be used to determine
// IA_NA  and IAADDR option formats.
TEST_F(OptionDefinitionTest, recognizeFormat) {
    // IA_NA option format.
    OptionDefinition opt_def1("OPTION_IA_NA", D6O_IA_NA, "record");
    for (int i = 0; i < 3; ++i) {
        opt_def1.addRecordField("uint32");
    }
    EXPECT_TRUE(opt_def1.haveIA6Format());
    // Create non-matching format to check that this function does not
    // return 'true' all the time.
    OptionDefinition opt_def2("OPTION_IA_NA", D6O_IA_NA, "uint16");
    EXPECT_FALSE(opt_def2.haveIA6Format());

    // IAADDR option format.
    OptionDefinition opt_def3("OPTION_IAADDR", D6O_IAADDR, "record");
    opt_def3.addRecordField("ipv6-address");
    opt_def3.addRecordField("uint32");
    opt_def3.addRecordField("uint32");
    EXPECT_TRUE(opt_def3.haveIAAddr6Format());
    // Create non-matching format to check that this function does not
    // return 'true' all the time.
    OptionDefinition opt_def4("OPTION_IAADDR", D6O_IAADDR, "uint32", true);
    EXPECT_FALSE(opt_def4.haveIAAddr6Format());
}

} // anonymous namespace