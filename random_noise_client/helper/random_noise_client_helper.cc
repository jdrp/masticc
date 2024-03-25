/*
 * Copyright (c) 2008 INRIA
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 */
#include "random_noise_client_helper.h"

#include "ns3/names.h"
#include "ns3/random_noise_client.h"
#include "ns3/uinteger.h"

namespace ns3
{

RandomNoiseClientHelper::RandomNoiseClientHelper(Address address, uint16_t port)
{
    m_factory.SetTypeId(RandomNoiseClient::GetTypeId());
    SetAttribute("RemoteAddress", AddressValue(address));
    SetAttribute("RemotePort", UintegerValue(port));
}

RandomNoiseClientHelper::RandomNoiseClientHelper(Address address)
{
    m_factory.SetTypeId(RandomNoiseClient::GetTypeId());
    SetAttribute("RemoteAddress", AddressValue(address));
}

void
RandomNoiseClientHelper::SetAttribute(std::string name, const AttributeValue& value)
{
    m_factory.Set(name, value);
}

void
RandomNoiseClientHelper::SetFill(Ptr<Application> app, std::string fill)
{
    app->GetObject<RandomNoiseClient>()->SetFill(fill);
}

void
RandomNoiseClientHelper::SetFill(Ptr<Application> app, uint8_t fill, uint32_t dataLength)
{
    app->GetObject<RandomNoiseClient>()->SetFill(fill, dataLength);
}

void
RandomNoiseClientHelper::SetFill(Ptr<Application> app,
                             uint8_t* fill,
                             uint32_t fillLength,
                             uint32_t dataLength)
{
    app->GetObject<RandomNoiseClient>()->SetFill(fill, fillLength, dataLength);
}

ApplicationContainer
RandomNoiseClientHelper::Install(Ptr<Node> node) const
{
    return ApplicationContainer(InstallPriv(node));
}

ApplicationContainer
RandomNoiseClientHelper::Install(std::string nodeName) const
{
    Ptr<Node> node = Names::Find<Node>(nodeName);
    return ApplicationContainer(InstallPriv(node));
}

ApplicationContainer
RandomNoiseClientHelper::Install(NodeContainer c) const
{
    ApplicationContainer apps;
    for (auto i = c.Begin(); i != c.End(); ++i)
    {
        apps.Add(InstallPriv(*i));
    }

    return apps;
}

Ptr<Application>
RandomNoiseClientHelper::InstallPriv(Ptr<Node> node) const
{
    Ptr<Application> app = m_factory.Create<RandomNoiseClient>();
    node->AddApplication(app);

    return app;
}

} // namespace ns3
