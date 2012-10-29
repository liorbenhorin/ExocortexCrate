#include "CommonSceneGraph.h"
#include "CommonLog.h"

struct PrintStackElement
{
   exoNodePtr eNode;
   int level;
   PrintStackElement(exoNodePtr enode, int l):eNode(enode), level(l)
   {}
};

void printSceneGraph(exoNodePtr root)
{
 

   ESS_LOG_WARNING("ExoSceneGraph Begin");

   std::list<PrintStackElement> sceneStack;
   
   sceneStack.push_back(PrintStackElement(root, 0));

   while( !sceneStack.empty() )
   {

      PrintStackElement sElement = sceneStack.back();
      exoNodePtr eNode = sElement.eNode;
      sceneStack.pop_back();

      ESS_LOG_WARNING("Level: "<<sElement.level<<" - Name: "<<eNode->name<<" - Selected: "<<(eNode->selected?"true":"false")<<" - path: "<<eNode->dccIdentifier);
         //<<" - identifer: "<<eNode->dccIdentifier);

      for( std::list<exoNodePtr>::iterator it = eNode->children.begin(); it != eNode->children.end(); it++){
         sceneStack.push_back(PrintStackElement(*it, sElement.level+1));
      }
   }

   ESS_LOG_WARNING("ExoSceneGraph End");
}

bool hasExtractableTransform( SceneNode::nodeTypeE type )
{
   return 
      type == SceneNode::CAMERA ||
      type == SceneNode::POLYMESH ||
      type == SceneNode::SUBD ||
      type == SceneNode::SURFACE ||
      type == SceneNode::CURVES ||
      type == SceneNode::PARTICLES;
}

struct SelectChildrenStackElement
{
   exoNodePtr eNode;
   bool bSelectChildren;
   SelectChildrenStackElement(exoNodePtr enode, bool selectChildren):eNode(enode), bSelectChildren(selectChildren)
   {}
};
void selectNodes(exoNodePtr root, SceneNode::SelectionT selectionMap, bool bSelectParents, bool bChildren, bool bSelectShapeNodes)
{


   std::list<SelectChildrenStackElement> sceneStack;
   
   sceneStack.push_back(SelectChildrenStackElement(root, false));

   while( !sceneStack.empty() )
   {
      SelectChildrenStackElement sElement = sceneStack.back();
      exoNodePtr eNode = sElement.eNode;
      sceneStack.pop_back();

      bool bSelected = false;
      if(selectionMap.find(eNode->dccIdentifier) != selectionMap.end() && 
         (eNode->type == SceneNode::ETRANSFORM || eNode->type == SceneNode::ITRANSFORM)){
         
         //this node's name matches one of the names from the selection map, so select it
         eNode->selected = true;

         if(eNode->type == SceneNode::ETRANSFORM && bSelectShapeNodes){
            for(std::list<exoNodePtr>::iterator it=eNode->children.begin(); it != eNode->children.end(); it++){
               if(::hasExtractableTransform((*it)->type)){
                  (*it)->selected = true;
                  break;
               }
            }
         }

         if(bSelectParents){// select all parent nodes
            exoNodePtr currNode = eNode->parent;
            while(currNode){
               currNode->selected = true;
               currNode = currNode->parent;
            }
         }

         if(bChildren){// select the children
            bSelected = true;
         }
      }
      if(sElement.bSelectChildren){
         bSelected = true;
         eNode->selected = true;
      }

      for( std::list<exoNodePtr>::iterator it = eNode->children.begin(); it != eNode->children.end(); it++){
         sceneStack.push_back(SelectChildrenStackElement(*it, bSelected));
      }
   }

   root->selected = true;
}


//void filterNodeSelection(exoNodePtr root, bool bExcludeNonTransforms)
//{
//   struct stackElement
//   {
//      exoNodePtr eNode;
//      stackElement(exoNodePtr enode):eNode(enode)
//      {}
//   };
//
//   std::list<stackElement> sceneStack;
//   
//   sceneStack.push_back(stackElement(root));
//
//   while( !sceneStack.empty() )
//   {
//      stackElement sElement = sceneStack.back();
//      exoNodePtr eNode = sElement.eNode;
//      sceneStack.pop_back();
//
//      if(bExcludeNonTransforms && 
//         (eNode->type != SceneNode::ITRANSFORM && eNode->type != SceneNode::ETRANSFORM && eNode->type != SceneNode::UNKNOWN)
//      ){
//         eNode->selected = false;
//      }
//
//      for( std::list<exoNodePtr>::iterator it = eNode->children.begin(); it != eNode->children.end(); it++){
//         sceneStack.push_back(stackElement(*it));
//      }
//   }
//
//   root->selected = true;
//}
