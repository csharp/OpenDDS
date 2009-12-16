/*
 * $Id$
 *
 * Copyright 2009 Object Computing, Inc.
 *
 * Distributed under the OpenDDS License.
 * See: http://www.opendds.org/license.html
 */

#include "MonitorDataStorage.h"

#include <sstream>

Monitor::MonitorDataStorage::MonitorDataStorage( MonitorData* model)
 : model_( model),
   publisherIdGenerator_( 0),
   subscriberIdGenerator_( 0),
   transportIdGenerator_( 0)
{
  this->reset();
}

Monitor::MonitorDataStorage::~MonitorDataStorage()
{
  this->clear();
}

void
Monitor::MonitorDataStorage::reset()
{
  this->clear();

  // Establish the generators.
  this->publisherIdGenerator_ = new RepoIdGenerator( 0, 0, OpenDDS::DCPS::KIND_PUBLISHER);
  this->subscriberIdGenerator_ = new RepoIdGenerator( 0, 0, OpenDDS::DCPS::KIND_SUBSCRIBER);
  this->transportIdGenerator_ = new RepoIdGenerator( 0, 0, OpenDDS::DCPS::KIND_USER);
}

void
Monitor::MonitorDataStorage::clear()
{
  // Empty the maps.
  this->guidToTreeMap_.clear();
  this->hostToTreeMap_.clear();
  this->processToTreeMap_.clear();
  this->instanceToTreeMap_.clear();
  this->transportToTreeMap_.clear();

  // Remove the generators.
  delete this->publisherIdGenerator_;
  delete this->subscriberIdGenerator_;
  delete this->transportIdGenerator_;
}

void
Monitor::MonitorDataStorage::cleanMaps( TreeNode* node)
{
  // Remove all our children from the maps first.
  for( int child = 0; child < node->size(); ++child) {
    this->cleanMaps( (*node)[ child]);
  }

  // Now remove ourselves from all the maps.

  std::pair< bool, GuidToTreeMap::key_type> gResult
    = this->findKey( this->guidToTreeMap_, node);
  if( gResult.first) {
    this->guidToTreeMap_.erase( gResult.second);
  }

  std::pair< bool, HostToTreeMap::key_type> hResult
    = this->findKey( this->hostToTreeMap_, node);
  if( hResult.first) {
    this->hostToTreeMap_.erase( hResult.second);
  }

  std::pair< bool, ProcessToTreeMap::key_type> pResult
    = this->findKey( this->processToTreeMap_, node);
  if( pResult.first) {
    this->processToTreeMap_.erase( pResult.second);
  }

  std::pair< bool, InstanceToTreeMap::key_type> iResult
    = this->findKey( this->instanceToTreeMap_, node);
  if( iResult.first) {
    this->instanceToTreeMap_.erase( iResult.second);
  }

  std::pair< bool, TransportToTreeMap::key_type> tResult
    = this->findKey( this->transportToTreeMap_, node);
  if( tResult.first) {
    this->transportToTreeMap_.erase( tResult.second);
  }
}

template< class MapType>
std::pair< bool, typename MapType::key_type>
Monitor::MonitorDataStorage::findKey( MapType& map, TreeNode* node)
{
  // This search is predicated on a node only being present once in any
  // tree.
  // Need to build a reverse index to do this efficiently.
  for( typename MapType::iterator current = map.begin();
       current != map.end();
       ++current) {
    if( node == current->second.second) {
      return std::make_pair( true, current->first);
    }
  }
  return std::make_pair( false, typename MapType::key_type());
}

Monitor::TreeNode*
Monitor::MonitorDataStorage::getProcessNode(
  const ProcessKey& key,
  bool& create
)
{
  // HOST

  TreeNode* hostNode = 0;
  HostToTreeMap::iterator hostLocation
    = this->hostToTreeMap_.find( key.host);
  if( hostLocation == this->hostToTreeMap_.end()) {
    // We are done if not creating a new node.
    if( !create) return 0;

    // We need to add a new host.  Host nodes are children of the
    // root node.
    TreeNode* root = this->model_->modelRoot();

    // Host first.
    QList<QVariant> list;
    list << QString("Host") << QString( QObject::tr( key.host.c_str()));
    hostNode = new TreeNode( list, root);
    root->append( hostNode);

    // Install the new node.
    this->hostToTreeMap_[ key.host]
      = std::make_pair( hostNode->row(), hostNode);

  } else {
    // Retain the current host node.
    hostNode = hostLocation->second.second;
  }

  // PROCESS

  TreeNode* pidNode = 0;
  ProcessToTreeMap::iterator pidLocation
    = this->processToTreeMap_.find( key);
  if( pidLocation == this->processToTreeMap_.end()) {
    // We are done if not creating a new node.
    if( !create) return 0;

    // We need to add a new PID.  PID nodes are children of the host
    // nodes.  We just found the relevant host node.

    // PID data.
    QList<QVariant> list;
    list << QString("Process") << QString::number( key.pid);
    pidNode = new TreeNode( list, hostNode);
    hostNode->append( pidNode);

    // Install the new node.
    this->processToTreeMap_[ key]
      = std::make_pair( pidNode->row(), pidNode);

  } else {
    pidNode = pidLocation->second.second;
    create = false;
  }

  return pidNode;
}

Monitor::TreeNode*
Monitor::MonitorDataStorage::getTransportNode(
  const TransportKey& key,
  bool&               create
)
{
  TreeNode* node = 0;
  TransportToTreeMap::iterator location
    = this->transportToTreeMap_.find( key);
  if( location == this->transportToTreeMap_.end()) {
    // We are done if not creating a new node.
    if( !create) return 0;

    // This transport needs to be installed.

    // Find the parent node, if any.  It is ok to not have a parent node
    // for cases of out-of-order updates.  We handle that as the updates
    // are actually processed.
    ProcessKey pid( key.host, key.pid);
    TreeNode* parent = this->getProcessNode( pid, create);

    QList<QVariant> list;
    QString value = QString("0x%1")
                    .arg( key.transport, 8, 16, QLatin1Char('0'));
    list << QString( QObject::tr( "Transport")) << value;
    node = new TreeNode( list, parent);
    if( parent) {
      parent->append( node);
    }

    // Install the new node.
    this->transportToTreeMap_[ key] = std::make_pair( node->row(), node);

  } else {
    node = location->second.second;
    create = false;
  }

  // If there have been some out-of-order reports, we may have been
  // created without a parent node.  We can fill in that information now
  // if we can.  If we created the node, we already know it has a
  // parent so this will be bypassed.
  if( !node->parent()) {
    create = true;
    ProcessKey pid( key.host, key.pid);
    node->parent() = this->getProcessNode( pid, create);
  }

//node->setColor(1,QColor("#ffbfbf"));
  return node;
}

Monitor::TreeNode*
Monitor::MonitorDataStorage::getParticipantNode(
  const ProcessKey&            pid,
  const OpenDDS::DCPS::GUID_t& id,
  bool&                        create
)
{
  TreeNode* node   = 0;
  GuidToTreeMap::iterator location = this->guidToTreeMap_.find( id);
  if( location == this->guidToTreeMap_.end()) {
    // We are done if not creating a new node.
    if( !create) return 0;

    // We need to add a new DomainParticipant.

    // Find the parent node, if any.  It is ok to not have a parent node
    // for cases of out-of-order udpates.  We handle that as the updates
    // are actually processed.
    TreeNode* parent = this->getProcessNode( pid, create);

    // DomainParticipant data.
    OpenDDS::DCPS::GuidConverter converter( id);
    QList<QVariant> list;
    list << QString("DomainParticipant")
         << QString( QObject::tr( std::string( converter).c_str()));
    node = new TreeNode( list, parent);
    if( parent) {
      parent->append( node);
    }

    // Install the new node.
    this->guidToTreeMap_[ id] = std::make_pair( node->row(), node);

  } else {
    node = location->second.second;
    create = false;
  }

  // If there have been some out-of-order reports, we may have been
  // created without a parent node.  We can now fill in that information
  // if we can.  If we created the node, we already know it has a
  // parent so this will be bypassed.
  if( !node->parent()) {
    create = true;
    node->parent() = this->getProcessNode( pid, create);
  }

  return node;
}

Monitor::TreeNode*
Monitor::MonitorDataStorage::getInstanceNode(
  const std::string& label,
  const InstanceKey& key,
  bool& create
)
{
  TreeNode* node   = 0;
  InstanceToTreeMap::iterator location
    = this->instanceToTreeMap_.find( key);
  if( location == this->instanceToTreeMap_.end()) {
    // We are done if not creating a new node.
    if( !create) return 0;

    // We need to add a new DomainParticipant.

    // Find the parent node, if any.  It is ok to not have a parent node
    // for cases of out-of-order udpates.  We handle that as the updates
    // are actually processed.
    TreeNode* parent = 0;
    GuidToTreeMap::iterator parentLocation
      = this->guidToTreeMap_.find( key.guid);
    if( parentLocation != this->guidToTreeMap_.end()) {
      parent = parentLocation->second.second;
    }

    // Node data.
    QList<QVariant> list;
    list << QString( QObject::tr( label.c_str()))
         << QString::number( key.handle);
    node = new TreeNode( list, parent);
    if( parent) {
      parent->append( node);
    }

    // Install the new node.
    this->instanceToTreeMap_[ key] = std::make_pair( node->row(), node);

  } else {
    node = location->second.second;
    create = false;
  }

  // If there have been some out-of-order reports, we may have been
  // created without a parent node.  We can now fill in that information
  // if we can.  If we created the node, we already know it has a
  // parent so this will be bypassed.
  if( !node->parent()) {
    // Need to search for the parent.
    TreeNode* parent = 0;
    GuidToTreeMap::iterator parentLocation
      = this->guidToTreeMap_.find( key.guid);
    if( parentLocation != this->guidToTreeMap_.end()) {
      parent = parentLocation->second.second;
    }

    // And install anything that is found.
    node->parent() = parent;
  }

  return node;
}

Monitor::TreeNode*
Monitor::MonitorDataStorage::getEndpointNode(
  const std::string&           label,
  const InstanceKey&           key,
  const OpenDDS::DCPS::GUID_t& id,
  bool&                        create
)
{
  TreeNode* node   = 0;
  GuidToTreeMap::iterator location
    = this->guidToTreeMap_.find( id);
  if( location == this->guidToTreeMap_.end()) {
    // We are done if not creating a new node.
    if( !create) return 0;

    // We need to add a new endpoint.

    // Find the parent node, if any.  It is ok to not have a parent node
    // for cases of out-of-order udpates.  We handle that as the updates
    // are actually processed.
    TreeNode* parent = 0;
    InstanceToTreeMap::iterator parentLocation
      = this->instanceToTreeMap_.find( key);
    if( parentLocation != this->instanceToTreeMap_.end()) {
      parent = parentLocation->second.second;
    }

    // Node data.
    OpenDDS::DCPS::GuidConverter converter( id);
    QList<QVariant> list;
    list << QString( QObject::tr( label.c_str()))
         << QString( QObject::tr( std::string( converter).c_str()));
    node = new TreeNode( list, parent);
    if( parent) {
      parent->append( node);
    }

    // Install the new node.
    this->guidToTreeMap_[ id] = std::make_pair( node->row(), node);

  } else {
    node = location->second.second;
    create = false;
  }

  // If there have been some out-of-order reports, we may have been
  // created without a parent node.  We can now fill in that information
  // if we can.  If we created the node, we already know it has a
  // parent so this will be bypassed.
  if( !node->parent()) {
    // Need to search for the parent.
    TreeNode* parent = 0;
    InstanceToTreeMap::iterator parentLocation
      = this->instanceToTreeMap_.find( key);
    if( parentLocation != this->instanceToTreeMap_.end()) {
      parent = parentLocation->second.second;
    }

    // And install anything that is found.
    node->parent() = parent;
  }

  return node;
}

Monitor::TreeNode*
Monitor::MonitorDataStorage::getNode(
  const std::string&           label,
  const OpenDDS::DCPS::GUID_t& parentId,
  const OpenDDS::DCPS::GUID_t& id,
  bool&                        create
)
{
  TreeNode* node   = 0;
  GuidToTreeMap::iterator location = this->guidToTreeMap_.find( id);
  if( location == this->guidToTreeMap_.end()) {
    // We are done if not creating a new node.
    if( !create) return 0;

    // We need to add a new DomainParticipant.

    // Find the parent node, if any.  It is ok to not have a parent node
    // for cases of out-of-order udpates.  We handle that as the updates
    // are actually processed.
    TreeNode* parent = 0;
    GuidToTreeMap::iterator parentLocation
      = this->guidToTreeMap_.find( parentId);
    if( parentLocation != this->guidToTreeMap_.end()) {
      parent = parentLocation->second.second;
    }

    // Node data.
    OpenDDS::DCPS::GuidConverter converter( id);
    QList<QVariant> list;
    list << QString( QObject::tr( label.c_str()))
         << QString( QObject::tr( std::string( converter).c_str()));
    node = new TreeNode( list, parent);
    if( parent) {
      parent->append( node);
    }

    // Install the new node.
    this->guidToTreeMap_[ id] = std::make_pair( node->row(), node);

  } else {
    node = location->second.second;
    create = false;
  }

  // If there have been some out-of-order reports, we may have been
  // created without a parent node.  We can now fill in that information
  // if we can.  If we created the node, we already know it has a
  // parent so this will be bypassed.
  if( !node->parent()) {
    // Need to search for the parent.
    TreeNode* parent = 0;
    GuidToTreeMap::iterator parentLocation
      = this->guidToTreeMap_.find( parentId);
    if( parentLocation != this->guidToTreeMap_.end()) {
      parent = parentLocation->second.second;
    }

    // And install anything that is found.
    node->parent() = parent;
  }

  return node;
}

void
Monitor::MonitorDataStorage::manageTransportLink(
  TreeNode* node,
  int       transport_id,
  bool&     create
)
{
  // Start by finding the participant node.
  TreeNode* processNode = node->parent();
  if( processNode) {
    // And follow it to the actual process node.
    processNode = processNode->parent();

  } else {
    // Horribly corrupt model, something oughta been done about it!
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) ERROR: MonitorDataStorage::manageTransportLink() - ")
      ACE_TEXT("unable to locate the ancestor process node!\n")
    ));
    return;
  }

  // Then finds its key in the maps.
  ProcessKey processKey;
  std::pair< bool, ProcessKey> pResult
    = this->findKey( this->processToTreeMap_, processNode);
  if( pResult.first) {
    processKey = pResult.second;

  } else {
    // Horribly corrupt model, something oughta been done about it!
    ACE_ERROR((LM_ERROR,
      ACE_TEXT("(%P|%t) ERROR: MonitorDataStorage::manageTransportLink() - ")
      ACE_TEXT("unable to locate the process node in the maps!\n")
    ));
    return;
  }

  // Now we have enough information to build a TransportKey and find or
  // create a transport node for our Id value.
  TransportKey transportKey(
                 processKey.host,
                 processKey.pid,
                 transport_id
               );
  TreeNode* transportNode = this->getTransportNode( transportKey, create);
  if( !node) {
    return;
  }

  // Transport Id display.
  QString label( QObject::tr( "Transport Id"));
  int row = node->indexOf( 0, label);
  if( row == -1) {
    // New entry, add a reference to the actual transport node.
    QList<QVariant> list;
    list << label << QString( QObject::tr("<error>"));;
    TreeNode* idNode = new TreeNode( list, node);
idNode->setColor( 1, QColor("#bfbfff"));
    node->append( idNode);
    transportNode->addValueRef( idNode);
transportNode->setColor( 1, QColor("#bfffbf"));
    create = true;
  }
}

void
Monitor::MonitorDataStorage::manageTopicLink(
  TreeNode*                    node,
  const OpenDDS::DCPS::GUID_t& dp_id,
  const OpenDDS::DCPS::GUID_t& topic_id,
  bool&                        create
)
{
  // This gets a bit tedious.  Here are the cases:
  // 1) We currently have no reference, and found a Topic node to
  //    reference:
  //    - attach a reference to the topic node;
  // 2) We currently have no reference, and found a Name node to
  //    reference:
  //    - attach a reference to the name node;
  // 3) We currently have a Topic node referenced, and found the same
  //    topic node to reference:
  //    - nothing to do;
  // 4) We currently have a Topic node referenced, and found a different
  //    Topic node to reference:
  //    - detach previous reference and reattach to new topic node;
  // 5) We currently have a Topic node referenced, but were able to find
  //    a name node to reference:
  //    - detach previous reference and reattach to new name node;
  // 6) We currently have a Name node referenced, and found the same name
  //    node to reference:
  //    - nothing to do;
  // 7) We currently have a Name node referenced, and found a different
  //    name node to reference:
  //    - detach previous reference and reattach to new name node;
  // 8) We currently have a Name node referenced, and found a different
  //    Topic node to reference:
  //    - detach previous reference and reattach to new topic node.
  //
  // Note that cases (4), (7), and (8) indicate inconsistent data reports
  // and are error conditions.  We chose to not handle these cases.
  // Cases (3) and (6) require no action, so the only code paths we need
  // to consider are for cases (1), (2), and (5).

  // Find the actual topic.
  TreeNode* topicNode = this->getNode(
                          std::string( "Topic"),
                          dp_id,
                          topic_id,
                          create
                        );
  if( !topicNode) {
    return;
  }

  // Find the topic name to reference instead of the GUID value.
  TreeNode* nameNode = 0;
  QString nameLabel( QObject::tr( "Topic Name"));
  int row = topicNode->indexOf( 0, nameLabel);
  if( row != -1) {
    nameNode = (*topicNode)[ row];
  }

  // Check for an existing Topic entry.
  QString topicLabel( QObject::tr( "Topic"));
  int topicRow = node->indexOf( 0, topicLabel);
  if( nameNode && topicRow != -1) {
    // Case 5: We have a topic reference and found a name reference,
    //         remove the existing topic reference
    TreeNode* topicRef = (*node)[ topicRow];
    if( topicRef && topicRef->valueSource()) {
      topicRef->valueSource()->removeValueRef( topicRef);
    }
//  node->removeChildren( topicRow, 1); // DEVELOPMENT: don't delete to show stale data.
  }

  // The node to install.
  TreeNode* refNode = nameNode;
  if( !refNode) {
    refNode = topicNode;
  }

  // Check to see if we need to create a name entry.
  int nameRow  = node->indexOf( 0, nameLabel);
  if( nameRow == -1) {
    // New entry, add a reference to the topic or its name.
    QList<QVariant> list;
    list << topicLabel << QString( QObject::tr("<error>"));
    TreeNode* idNode = new TreeNode( list, node);
idNode->setColor( 1, QColor("#bfbfff"));
    node->append( idNode);
    refNode->addValueRef( idNode);
refNode->setColor( 1, QColor("#bfffbf"));
    create = true;
  }
}

void
Monitor::MonitorDataStorage::deleteProcessNode( TreeNode* node)
{
  std::pair< bool, ProcessToTreeMap::key_type> pResult
    = this->findKey( this->processToTreeMap_, node);
  if( pResult.first) {
    this->processToTreeMap_.erase( pResult.second);
  }

  // Remove the process from the host.
  TreeNode* hostNode = node->parent();
  hostNode->removeChildren( node->row(), 1);

  // Check and remove the host node if there are no pid nodes remaining
  // after the removal (no children).
  if( hostNode->size() == 0) {
    std::pair< bool, HostToTreeMap::key_type> hResult
      = this->findKey( this->hostToTreeMap_, hostNode);
    if( hResult.first) {
      this->hostToTreeMap_.erase( hResult.second);
    }
    delete hostNode;
  }

  delete node;

  // Notify the GUI to update.
  this->model_->changed();
}

void
Monitor::MonitorDataStorage::displayNvp(
  TreeNode*                    parent,
  const OpenDDS::DCPS::NVPSeq& data,
  bool                         layoutChanged,
  bool                         dataChanged
)
{
  // NAME / VALUE DATA
  int size = data.length();
  for( int index = 0; index < size; ++index) {
    QString name( data[ index].name);
    int row = parent->indexOf( 0, name);
    if( row == -1) {
      // This is new data, insert it.
      QList<QVariant> list;
      list << name;
      switch( data[ index].value._d()) {
        case OpenDDS::DCPS::INTEGER_TYPE:
          list << QString::number( data[ index].value.integer_value());
          break;

        case OpenDDS::DCPS::DOUBLE_TYPE:
          list << QString::number( data[ index].value.double_value());
          break;

        case OpenDDS::DCPS::STRING_TYPE:
          list << QString( data[ index].value.string_value());
          break;

        case OpenDDS::DCPS::STATISTICS_TYPE:
        case OpenDDS::DCPS::STRING_LIST_TYPE:
          list << QString( QObject::tr("<display unimplemented>"));
          break;
      }
      TreeNode* node = new TreeNode( list, parent);
      parent->append( node);
      layoutChanged = true;

    } else {
      // This is existing data, update the value.
      TreeNode* node = (*parent)[ row];
      switch( data[ index].value._d()) {
        case OpenDDS::DCPS::INTEGER_TYPE:
          node->setData( 1, QString::number( data[ index].value.integer_value()));
          break;

        case OpenDDS::DCPS::DOUBLE_TYPE:
          node->setData( 1, QString::number( data[ index].value.double_value()));
          break;

        case OpenDDS::DCPS::STRING_TYPE:
          node->setData( 1, QString( data[ index].value.string_value()));
          break;

        case OpenDDS::DCPS::STATISTICS_TYPE:
        case OpenDDS::DCPS::STRING_LIST_TYPE:
          break;
      }
      dataChanged = true;
    }
  }

  // Notify the GUI if we have changed the underlying model.
  if( layoutChanged) {
    /// @TODO: Check that we really do not need to do updated here.
    this->model_->changed();

  } else if( dataChanged) {
    this->model_->updated( parent, 1, (*parent)[ parent->size()-1], 1);
  }
}

std::string&
Monitor::MonitorDataStorage::activeIor()
{
  return this->activeIor_;
}

std::string
Monitor::MonitorDataStorage::activeIor() const
{
  return this->activeIor_;
}

bool
Monitor::MonitorDataStorage::ProcessKey::operator<( const ProcessKey& rhs) const
{
  if( this->host < rhs.host)      return true;
  else if( rhs.host < this->host) return false;
  else                            return this->pid < rhs.pid;
}

bool
Monitor::MonitorDataStorage::TransportKey::operator<( const TransportKey& rhs) const
{
  if( this->host < rhs.host)      return true;
  else if( rhs.host < this->host) return false;
  else if( this->pid < rhs.pid)   return true;
  else if( rhs.pid < this->pid)   return false;
  else                            return this->transport < rhs.transport;
}

bool
Monitor::MonitorDataStorage::InstanceKey::operator<( const InstanceKey& rhs) const
{
  GUID_tKeyLessThan compare;
  if( compare( this->guid, rhs.guid))      return true;
  else if( compare( rhs.guid, this->guid)) return false;
  else                                     return this->handle < rhs.handle;
}

